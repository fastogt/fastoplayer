/*  Copyright (C) 2014-2022 FastoGT. All right reserved.

    This file is part of FastoTV.

    FastoTV is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    FastoTV is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with FastoTV. If not, see <http://www.gnu.org/licenses/>.
*/

#include <player/media/video_state.h>

extern "C" {
#include <libavcodec/avcodec.h>        // for AVCodecContext, AVCode...
#include <libavcodec/version.h>        // for FF_API_EMU_EDGE
#include <libavformat/avio.h>          // for AVIOContext, AVIOInter...
#include <libavutil/avstring.h>        // for av_strlcatf
#include <libavutil/avutil.h>          // for AVMediaType::AVMEDIA_T...
#include <libavutil/buffer.h>          // for av_buffer_ref
#include <libavutil/channel_layout.h>  // for av_get_channel_layout_...
#include <libavutil/dict.h>            // for av_dict_free, av_dict_get
#include <libavutil/error.h>           // for AVERROR, AVERROR_EOF
#include <libavutil/mathematics.h>     // for av_compare_ts, av_resc...
#include <libavutil/mem.h>             // for av_freep, av_fast_malloc
#include <libavutil/opt.h>             // for AV_OPT_SEARCH_CHILDREN
#include <libavutil/pixdesc.h>         // for AVPixFmtDescriptor
#include <libavutil/pixfmt.h>          // for AVPixelFormat, AVPixel...
#include <libavutil/rational.h>        // for AVRational
#include <libavutil/samplefmt.h>       // for AVSampleFormat, av_sam...
#include <libswresample/swresample.h>  // for swr_free, swr_init

#if CONFIG_AVFILTER
#include <libavfilter/avfilter.h>    // for avfilter_graph_free
#include <libavfilter/buffersink.h>  // for av_buffersink_get_fram...
#include <libavfilter/buffersrc.h>   // for av_buffersrc_add_frame
#endif
}

#include <common/sprintf.h>
#include <common/threads/thread_manager.h>  // for THREAD_MANAGER
#include <common/utils.h>                   // for freeifnotnull

#include <player/media/ffmpeg_internal.h>

#include <player/media/app_options.h>  // for ComplexOptions, AppOpt...
#include <player/media/av_utils.h>
#include <player/media/decoder.h>  // for VideoDecoder, AudioDec...
#include <player/media/hwaccels/ffmpeg_hw.h>
#include <player/media/packet_queue.h>  // for PacketQueue
#include <player/media/stream.h>        // for AudioStream, VideoStream
#include <player/media/types.h>         // for clock64_t, IsValidClock
#include <player/media/video_state_handler.h>

#include <player/media/frames/audio_frame.h>  // for AudioFrame
#include <player/media/frames/frame_queue.h>  // for VideoDecoder, AudioDec...
#include <player/media/frames/video_frame.h>  // for VideoFrame

/* no AV sync correction is done if below the minimum AV sync threshold */
#define AV_SYNC_THRESHOLD_MIN_MSEC 40
/* AV sync correction is done if above the maximum AV sync threshold */
#define AV_SYNC_THRESHOLD_MAX_MSEC 100
/* If a frame duration is longer than this, it will not be duplicated to
 * compensate AV sync */
#define AV_SYNC_FRAMEDUP_THRESHOLD_MSEC 100

#define AV_NOSYNC_THRESHOLD_MSEC 10000

/* maximum audio speed change to get correct sync */
#define SAMPLE_CORRECTION_PERCENT_MAX 10
/* we use about AUDIO_DIFF_AVG_NB A-V differences to make the average */
#define AUDIO_DIFF_AVG_NB 20

/* NOTE: the size must be big enough to compensate the hardware audio buffersize
 * size */
/* TODO: We assume that a decoded and resampled frame fits into this buffer */
#define MAX_QUEUE_SIZE (15 * 1024 * 1024)

#define EXIT_LOOKUP_IF_HWACCEL_FAILED 0

namespace {
std::string ffmpeg_errno_to_string(int err) {
  char errbuf[128];
  if (av_strerror(err, errbuf, sizeof(errbuf)) < 0) {
    return strerror(AVUNERROR(err));
  }
  return errbuf;
}

bool cmp_audio_fmts(enum AVSampleFormat fmt1,
                    int64_t channel_count1,
                    enum AVSampleFormat fmt2,
                    int64_t channel_count2) {
  /* If channel count == 1, planar and non-planar formats are the same */
  if (channel_count1 == 1 && channel_count2 == 1) {
    return av_get_packed_sample_fmt(fmt1) != av_get_packed_sample_fmt(fmt2);
  }

  return channel_count1 != channel_count2 || fmt1 != fmt2;
}
}  // namespace

namespace fastoplayer {
namespace media {
namespace {

enum AVPixelFormat get_format(AVCodecContext* s, const enum AVPixelFormat* pix_fmts) {
  InputStream* ist = static_cast<InputStream*>(s->opaque);
  const enum AVPixelFormat* p;
  for (p = pix_fmts; *p != AV_PIX_FMT_NONE; p++) {
    const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(*p);
    if (!(desc->flags & AV_PIX_FMT_FLAG_HWACCEL)) {
      break;
    }

    const AVCodecHWConfig* config = nullptr;
    if (ist->hwaccel_id == HWACCEL_GENERIC || ist->hwaccel_id == HWACCEL_AUTO) {
      for (int i = 0;; i++) {
        config = avcodec_get_hw_config(s->codec, i);
        if (!config) {
          break;
        }
        if (!(config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)) {
          continue;
        }
        if (config->pix_fmt == *p) {
          break;
        }
      }
    }

    if (config) {
      if (config->device_type != ist->hwaccel_device_type) {
        // Different hwaccel offered, ignore.
        continue;
      }

      int ret = hwaccel_decode_init(s);
      if (ret < 0) {
        if (ist->hwaccel_id == HWACCEL_GENERIC) {
#if EXIT_LOOKUP_IF_HWACCEL_FAILED
          return AV_PIX_FMT_NONE;
#else
          continue;
#endif
        }
        continue;
      }
      ist->active_hwaccel_id = config->device_type;
    } else {
      const HWAccel* hwaccel = get_hwaccel(*p);
      if (!hwaccel || (ist->hwaccel_id != HWACCEL_AUTO && ist->hwaccel_id != hwaccel->id)) {
        continue;
      }

      int ret = hwaccel->init(s);
      if (ret < 0) {
        if (ist->hwaccel_id == hwaccel->id) {
#if EXIT_LOOKUP_IF_HWACCEL_FAILED
          return AV_PIX_FMT_NONE;
#else
          continue;
#endif
        }
        continue;
      }
      ist->active_hwaccel_id = hwaccel->hid;
    }

    if (ist->hw_frames_ctx) {
      s->hw_frames_ctx = av_buffer_ref(ist->hw_frames_ctx);
      if (!s->hw_frames_ctx) {
#if EXIT_LOOKUP_IF_HWACCEL_FAILED
        return AV_PIX_FMT_NONE;
#else
        continue;
#endif
      }
    }

    ist->hwaccel_pix_fmt = *p;
    break;
  }

  return *p;
}

int get_buffer(AVCodecContext* s, AVFrame* frame, int flags) {
  InputStream* ist = static_cast<InputStream*>(s->opaque);

  if (ist->hwaccel_get_buffer && frame->format == ist->hwaccel_pix_fmt) {
    return ist->hwaccel_get_buffer(s, frame, flags);
  }

  return avcodec_default_get_buffer2(s, frame, flags);
}
}  // namespace

VideoState::VideoState(stream_id id, const common::uri::GURL& uri, const AppOptions& opt, const ComplexOptions& copt)
    : id_(id),
      uri_(uri),
      opt_(opt),
      copt_(copt),
      force_refresh_(false),
      read_pause_return_(0),
      ic_(nullptr),
      realtime_(false),
      vstream_(new VideoStream),
      astream_(new AudioStream),
      viddec_(nullptr),
      auddec_(nullptr),
      video_frame_queue_(nullptr),
      audio_frame_queue_(nullptr),
      audio_clock_(0),
      audio_diff_cum_(0),
      audio_diff_avg_coef_(0),
      audio_diff_threshold_(0),
      audio_diff_avg_count_(0),
      audio_hw_buf_size_(0),
      audio_buf_(nullptr),
      audio_buf1_(nullptr),
      audio_buf_size_(0),
      audio_buf1_size_(0),
      audio_buf_index_(0),
      audio_write_buf_size_(0),
      audio_src_(),
#if CONFIG_AVFILTER
      audio_filter_src_(),
#endif
      audio_tgt_(),
      swr_ctx_(nullptr),
      frame_timer_(0),
      frame_last_returned_time_(0),
      frame_last_filter_delay_(0),
      max_frame_duration_(0),
      step_(false),
#if CONFIG_AVFILTER
      in_video_filter_(nullptr),
      out_video_filter_(nullptr),
      in_audio_filter_(nullptr),
      out_audio_filter_(nullptr),
      agraph_(nullptr),
#endif
      last_video_stream_(invalid_stream_index),
      last_audio_stream_(invalid_stream_index),
      vdecoder_tid_(THREAD_MANAGER()->CreateThread(&VideoState::VideoThread, this)),
      adecoder_tid_(THREAD_MANAGER()->CreateThread(&VideoState::AudioThread, this)),
      paused_(false),
      last_paused_(false),
      eof_(false),
      abort_request_(false),
      stats_(new Stats),
      handler_(nullptr),
      input_st_(static_cast<InputStream*>(calloc(1, sizeof(InputStream)))),
      seek_req_(false),
      seek_pos_(0),
      seek_rel_(0),
      seek_flags_(0),
      read_thread_cond_(),
      read_thread_mutex_() {
  CHECK(id_ != invalid_stream_id);

  input_st_->hwaccel_id = opt_.hwaccel_id;
  input_st_->hwaccel_device = common::utils::strdupornull(opt_.hwaccel_device);
  input_st_->hwaccel_device_type = static_cast<AVHWDeviceType>(opt_.hwaccel_device_type);
  if (!opt_.hwaccel_output_format.empty()) {
    const char* hwaccel_output_format = opt_.hwaccel_output_format.c_str();
    input_st_->hwaccel_output_format = av_get_pix_fmt(hwaccel_output_format);
    if (input_st_->hwaccel_output_format == AV_PIX_FMT_NONE) {
      CRITICAL_LOG() << "Unrecognised hwaccel output format: " << hwaccel_output_format;
    }
  } else {
    input_st_->hwaccel_output_format = AV_PIX_FMT_NONE;
  }
}

VideoState::~VideoState() {
  destroy(&astream_);
  destroy(&vstream_);

  common::utils::freeifnotnull(input_st_->hwaccel_device);
  free(input_st_);
  input_st_ = nullptr;
}

void VideoState::SetHandler(VideoStateHandler* handler) {
  handler_ = handler;
}

int VideoState::StreamComponentOpen(int stream_index) {
  if (stream_index == invalid_stream_index || static_cast<unsigned int>(stream_index) >= ic_->nb_streams) {
    return AVERROR(EINVAL);
  }

  AVStream* stream = ic_->streams[stream_index];
  const char* forced_codec_name = nullptr;
  AVCodecParameters* par = stream->codecpar;
  enum AVCodecID codec_id = par->codec_id;
  const AVCodec* codec = avcodec_find_decoder(codec_id);

  if (par->codec_type == AVMEDIA_TYPE_VIDEO) {
    last_video_stream_ = stream_index;
    forced_codec_name = opt_.video_codec_name.empty() ? nullptr : opt_.video_codec_name.c_str();
  } else if (par->codec_type == AVMEDIA_TYPE_AUDIO) {
    last_audio_stream_ = stream_index;
    forced_codec_name = opt_.audio_codec_name.empty() ? nullptr : opt_.audio_codec_name.c_str();
  }
  if (forced_codec_name) {
    codec = avcodec_find_decoder_by_name(forced_codec_name);
  }
  if (!codec) {
    if (forced_codec_name) {
      WARNING_LOG() << "No codec could be found with name '" << forced_codec_name << "'";
    } else {
      WARNING_LOG() << "No codec could be found with id " << codec_id;
    }
    return AVERROR(EINVAL);
  }

  AVCodecContext* avctx = avcodec_alloc_context3(codec);
  if (!avctx) {
    return AVERROR(ENOMEM);
  }

  int ret = avcodec_parameters_to_context(avctx, par);
  if (ret < 0) {
    avcodec_free_context(&avctx);
    return ret;
  }

  AVRational tb = stream->time_base;
  avctx->pkt_timebase = tb;

  int stream_lowres = opt_.lowres;
  avctx->codec_id = codec->id;
  if (stream_lowres > codec->max_lowres) {
    WARNING_LOG() << "The maximum value for lowres supported by the decoder is " << codec->max_lowres;
    stream_lowres = codec->max_lowres;
  }
  avctx->lowres = stream_lowres;

#if FF_API_EMU_EDGE
  if (stream_lowres) {
    avctx->flags |= CODEC_FLAG_EMU_EDGE;
  }
#endif
  if (opt_.fast) {
    avctx->flags2 |= AV_CODEC_FLAG2_FAST;
  }
#if FF_API_EMU_EDGE
  if (codec->capabilities & AV_CODEC_CAP_DR1) {
    avctx->flags |= CODEC_FLAG_EMU_EDGE;
  }
#endif

  if (avctx->codec_type == AVMEDIA_TYPE_VIDEO) {
    avctx->opaque = input_st_;
    avctx->get_format = get_format;
    avctx->get_buffer2 = get_buffer;
    avctx->thread_safe_callbacks = 1;
  }

  AVDictionary* opts = filter_codec_opts(copt_.codec_opts, avctx->codec_id, ic_, stream, codec);
  if (!av_dict_get(opts, "threads", nullptr, 0)) {
    av_dict_set(&opts, "threads", "auto", 0);
  }
  if (stream_lowres) {
    av_dict_set_int(&opts, "lowres", stream_lowres, 0);
  }
  if (avctx->codec_type == AVMEDIA_TYPE_VIDEO || avctx->codec_type == AVMEDIA_TYPE_AUDIO) {
    av_dict_set(&opts, "refcounted_frames", "1", 0);
  }

  if (avctx->codec_type == AVMEDIA_TYPE_VIDEO) {
    ret = hw_device_setup_for_decode(avctx, const_cast<AVCodec*>(codec));
    if (ret < 0) {
#if EXIT_LOOKUP_IF_HWACCEL_FAILED
      avcodec_free_context(&avctx);
      av_dict_free(&opts);
      return ret;
#else
      input_st_->hwaccel_id = HWACCEL_NONE;
#endif
    }
  }

  ret = avcodec_open2(avctx, codec, &opts);
  if (ret < 0) {
    avcodec_free_context(&avctx);
    av_dict_free(&opts);
    return ret;
  }

  AVDictionaryEntry* t = av_dict_get(opts, "", nullptr, AV_DICT_IGNORE_SUFFIX);
  if (t) {
    ERROR_LOG() << "Option " << t->key << " not found.";
    avcodec_free_context(&avctx);
    av_dict_free(&opts);
    return AVERROR_OPTION_NOT_FOUND;
  }

  int sample_rate, nb_channels;
  int64_t channel_layout = 0;
  eof_ = false;
  stream->discard = AVDISCARD_DEFAULT;
  if (avctx->codec_type == AVMEDIA_TYPE_VIDEO) {
    AVRational frame_rate = av_guess_frame_rate(ic_, stream, nullptr);
    bool opened = vstream_->Open(stream_index, stream, frame_rate);
    UNUSED(opened);
    PacketQueue* packet_queue = vstream_->GetQueue();
    video_frame_queue_ = new video_frame_queue_t;
    viddec_ = new VideoDecoder(avctx, packet_queue);
    viddec_->Start();
    if (!vdecoder_tid_->Start()) {
      destroy(&viddec_);
      goto out;
    }
  } else if (avctx->codec_type == AVMEDIA_TYPE_AUDIO) {
#if CONFIG_AVFILTER
    {
      audio_filter_src_.freq = avctx->sample_rate;
      audio_filter_src_.channels = avctx->channels;
      audio_filter_src_.channel_layout = get_valid_channel_layout(avctx->channel_layout, avctx->channels);
      audio_filter_src_.fmt = avctx->sample_fmt;
      ret = ConfigureAudioFilters(opt_.afilters, 0);
      if (ret < 0) {
        avcodec_free_context(&avctx);
        av_dict_free(&opts);
        return ret;
      }
      AVFilterContext* sink = out_audio_filter_;
      sample_rate = av_buffersink_get_sample_rate(sink);
      nb_channels = av_buffersink_get_channels(sink);
      channel_layout = av_buffersink_get_channel_layout(sink);
    }
#else
    sample_rate = avctx->sample_rate;
    nb_channels = avctx->channels;
    channel_layout = avctx->channel_layout;
#endif

    int audio_buff_size = 0;
    if (!handler_) {
      avcodec_free_context(&avctx);
      av_dict_free(&opts);
      return -1;
    }

    common::Error err =
        handler_->HandleRequestAudio(this, channel_layout, nb_channels, sample_rate, &audio_tgt_, &audio_buff_size);
    if (err) {
      avcodec_free_context(&avctx);
      av_dict_free(&opts);
      return ret;
    }

    audio_hw_buf_size_ = audio_buff_size;
    audio_src_ = audio_tgt_;
    audio_buf_size_ = 0;
    audio_buf_index_ = 0;

    /* init averaging filter */
    audio_diff_avg_coef_ = exp(log(0.01) / AUDIO_DIFF_AVG_NB);
    DCHECK(0 <= audio_diff_avg_coef_ && audio_diff_avg_coef_ <= 1);
    audio_diff_avg_count_ = 0;
    /* since we do not have a precise anough audio FIFO fullness,
       we correct audio sync only if larger than this threshold */
    audio_diff_threshold_ =
        static_cast<double>(audio_hw_buf_size_) / static_cast<double>(audio_tgt_.bytes_per_sec) * 1000;
    bool opened = astream_->Open(stream_index, stream);
    UNUSED(opened);
    PacketQueue* packet_queue = astream_->GetQueue();
    audio_frame_queue_ = new audio_frame_queue_t;
    auddec_ = new AudioDecoder(avctx, packet_queue);
    if ((ic_->iformat->flags & (AVFMT_NOBINSEARCH | AVFMT_NOGENSEARCH | AVFMT_NO_BYTE_SEEK)) &&
        !ic_->iformat->read_seek) {
      auddec_->SetStartPts(stream->start_time, stream->time_base);
    }
    auddec_->Start();
    if (!adecoder_tid_->Start()) {
      destroy(&auddec_);
      goto out;
    }
  }
out:
  av_dict_free(&opts);
  return ret;
}

void VideoState::StreamComponentClose(int stream_index) {
  if (stream_index < 0 || static_cast<unsigned int>(stream_index) >= ic_->nb_streams) {
    return;
  }

  AVStream* avs = ic_->streams[stream_index];
  AVCodecParameters* codecpar = avs->codecpar;
  if (codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
    if (video_frame_queue_) {
      video_frame_queue_->Stop();
    }
    if (viddec_) {
      viddec_->Abort();
    }
    if (vdecoder_tid_) {
      vdecoder_tid_->Join();
      vdecoder_tid_ = nullptr;
    }
    if (input_st_->hwaccel_uninit) {
      input_st_->hwaccel_uninit(viddec_->GetAvCtx());
      input_st_->hwaccel_uninit = nullptr;
    }
    destroy(&viddec_);
    destroy(&video_frame_queue_);
  } else if (codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
    if (audio_frame_queue_) {
      audio_frame_queue_->Stop();
    }
    auddec_->Abort();
    if (adecoder_tid_) {
      adecoder_tid_->Join();
      adecoder_tid_ = nullptr;
    }
    destroy(&auddec_);
    destroy(&audio_frame_queue_);
    swr_free(&swr_ctx_);
    av_freep(&audio_buf1_);
    audio_buf1_size_ = 0;
    audio_buf_ = nullptr;
  }
  avs->discard = AVDISCARD_ALL;
}

void VideoState::StepToNextFrame() {
  /* if the stream is paused unpause it, then step */
  if (paused_) {
    StreamTogglePause();
  }
  step_ = true;
}

void VideoState::SeekNextChunk() {
  clock64_t incr = 0;
  if (ic_->nb_chapters <= 1) {
    incr = 60000;
  }
  SeekChapter(1);
  Seek(incr);
}

void VideoState::SeekPrevChunk() {
  clock64_t incr = 0;
  if (ic_->nb_chapters <= 1) {
    incr = -60000;
  }
  SeekChapter(-1);
  Seek(incr);
}

void VideoState::SeekChapter(int incr) {
  if (!ic_->nb_chapters) {
    return;
  }

  const AVRational tb = {1, AV_TIME_BASE};  // AV_TIME_BASE_Q
  const clock64_t pos = GetMasterClock() * AV_TIME_BASE * 1000;
  int i;
  /* find the current chapter */
  for (i = 0; i < ic_->nb_chapters; i++) {
    AVChapter* ch = ic_->chapters[i];
    if (av_compare_ts(pos, tb, ch->start, ch->time_base) < 0) {
      i--;
      break;
    }
  }

  i += incr;
  unsigned int chapter = FFMAX(i, 0);
  if (chapter >= ic_->nb_chapters) {
    return;
  }

  DEBUG_LOG() << "Seeking to chapter " << chapter << ".";
  int64_t poss = av_rescale_q(ic_->chapters[chapter]->start, ic_->chapters[chapter]->time_base, tb);
  StreamSeek(poss, 0, false);
}

void VideoState::StreamSeek(int64_t pos, int64_t rel, bool seek_by_bytes) {
  if (seek_req_) {
    return;
  }

  seek_pos_ = pos;
  seek_rel_ = rel;
  seek_flags_ &= ~AVSEEK_FLAG_BYTE;
  if (seek_by_bytes) {
    seek_flags_ |= AVSEEK_FLAG_BYTE;
  }
  seek_req_ = true;
  read_thread_cond_.notify_one();
}

void VideoState::Seek(clock64_t msec) {
  if (opt_.seek_by_bytes == SEEK_BY_BYTES_ON) {
    int64_t pos = -1;
    if (pos < 0 && vstream_->IsOpened()) {
      pos = video_frame_queue_->GetLastPos();
    }
    if (pos < 0 && astream_->IsOpened()) {
      pos = audio_frame_queue_->GetLastPos();
    }
    if (pos < 0) {
      pos = avio_tell(ic_->pb);
    }

    int64_t incr_in_bytes = 0;
    if (ic_->bit_rate) {
      incr_in_bytes = (msec / 1000) * ic_->bit_rate / 8.0;
    } else {
      incr_in_bytes = (msec / 1000) * 180000.0;
    }
    pos += incr_in_bytes;
    StreamSeek(pos, incr_in_bytes, true);
    return;
  }

  SeekMsec(msec);
}

void VideoState::SeekMsec(clock64_t clock) {
  clock64_t pos = GetMasterClock();
  if (!IsValidClock(pos)) {
    pos = seek_pos_ / AV_TIME_BASE * 1000;
  }
  pos += clock;
  if (ic_->start_time != AV_NOPTS_VALUE) {  // if selected out of range move to start
    clock64_t st = ic_->start_time / AV_TIME_BASE * 1000;
    if (pos < st) {
      pos = st;
    }
  }

  int64_t pos_seek = pos * AV_TIME_BASE / 1000;
  int64_t incr_seek = clock * AV_TIME_BASE / 1000;
  StreamSeek(pos_seek, incr_seek, false);
}

AvSyncType VideoState::GetMasterSyncType() const {
  return opt_.av_sync_type;
}

clock64_t VideoState::ComputeTargetDelay(clock64_t delay) const {
  clock64_t diff = 0;

  /* update delay to follow master synchronisation source */
  if (GetMasterSyncType() != AV_SYNC_VIDEO_MASTER) {
    /* if video is slave, we try to correct big delays by
       duplicating or deleting a frame */
    diff = vstream_->GetClock() - GetMasterClock();

    /* skip or repeat frame. We take into account the
       delay to compute the threshold. I still don't know
       if it is the best guess */
    clock64_t sync_threshold = FFMAX(AV_SYNC_THRESHOLD_MIN_MSEC, FFMIN(AV_SYNC_THRESHOLD_MAX_MSEC, delay));
    if (IsValidClock(diff) && std::abs(diff) < max_frame_duration_) {
      if (diff <= -sync_threshold) {
        delay = FFMAX(0, delay + diff);
      } else if (diff >= sync_threshold && delay > AV_SYNC_FRAMEDUP_THRESHOLD_MSEC) {
        delay = delay + diff;
      } else if (diff >= sync_threshold) {
        delay = 2 * delay;
      }
    }
  }
  DEBUG_LOG() << "video: delay=" << delay << " A-V=" << -diff;
  return delay;
}

clock64_t VideoState::GetMasterPts() const {
  if (GetMasterSyncType() == AV_SYNC_VIDEO_MASTER) {
    return vstream_->GetPts();
  }

  return astream_->GetPts();
}

/* get the current master clock value */
clock64_t VideoState::GetMasterClock() const {
  if (GetMasterSyncType() == AV_SYNC_VIDEO_MASTER) {
    return vstream_->GetClock();
  }

  return astream_->GetClock();
}

int VideoState::Exec() {
  int res = ReadRoutine();
  Close();
  avformat_close_input(&ic_);
  return res;
}

void VideoState::Abort() {
  abort_request_ = true;
}

bool VideoState::IsVideoThread() const {
  return common::threads::IsCurrentThread(vdecoder_tid_.get());
}

bool VideoState::IsAudioThread() const {
  return common::threads::IsCurrentThread(adecoder_tid_.get());
}

bool VideoState::IsAborted() const {
  return abort_request_;
}

bool VideoState::IsStreamReady() const {
  if (!astream_ && !vstream_) {
    return false;
  }

  return IsAudioReady() && IsVideoReady() && !IsAborted();
}

stream_id VideoState::GetId() const {
  return id_;
}

const common::uri::GURL& VideoState::GetUri() const {
  return uri_;
}

void VideoState::StreamTogglePause() {
  if (paused_) {
    frame_timer_ += GetRealClockTime() - vstream_->LastUpdatedClock();
    if (read_pause_return_ != AVERROR(ENOSYS)) {
      vstream_->SetPaused(false);
    }
    vstream_->SyncSerialClock();
  }
  paused_ = !paused_;
  vstream_->SetPaused(paused_);
  astream_->SetPaused(paused_);
}

void VideoState::RefreshRequest() {
  force_refresh_ = true;
}

void VideoState::TogglePause() {
  StreamTogglePause();
  step_ = false;
}

bool VideoState::IsPaused() const {
  return paused_;
}

void VideoState::ResetStats() {
  stats_.reset(new Stats);
}

void VideoState::Close() {
  /* close each stream */
  if (vstream_->IsOpened()) {
    StreamComponentClose(vstream_->Index());
    vstream_->Close();
  }
  if (astream_->IsOpened()) {
    StreamComponentClose(astream_->Index());
    astream_->Close();
  }
}

bool VideoState::IsAudioReady() const {
  return astream_ && (astream_->IsOpened() || !opt_.enable_audio);
}

bool VideoState::IsVideoReady() const {
  return vstream_ && (vstream_->IsOpened() || !opt_.enable_video);
}

void VideoState::StreamCycleChannel(AVMediaType codec_type) {
  int start_index = invalid_stream_index;
  int old_index = invalid_stream_index;
  if (codec_type == AVMEDIA_TYPE_VIDEO) {
    start_index = last_video_stream_;
    old_index = vstream_->Index();
  } else if (codec_type == AVMEDIA_TYPE_AUDIO) {
    start_index = last_audio_stream_;
    old_index = astream_->Index();
  } else {
    DNOTREACHED();
  }
  int stream_index = start_index;

  AVProgram* p = nullptr;
  int lnb_streams = ic_->nb_streams;
  if (codec_type != AVMEDIA_TYPE_VIDEO && vstream_->IsOpened()) {
    p = av_find_program_from_stream(ic_, nullptr, old_index);
    if (p) {
      lnb_streams = p->nb_stream_indexes;
      for (start_index = 0; start_index < lnb_streams; start_index++) {
        if (p->stream_index[start_index] == stream_index) {
          break;
        }
      }
      if (start_index == lnb_streams) {
        start_index = invalid_stream_index;
      }
      stream_index = start_index;
    }
  }

  while (true) {
    if (++stream_index >= lnb_streams) {
      if (start_index == invalid_stream_index) {
        return;
      }
      stream_index = 0;
    }
    if (stream_index == start_index) {
      return;
    }
    AVStream* st = ic_->streams[p ? p->stream_index[stream_index] : stream_index];
    if (st->codecpar->codec_type == codec_type) {
      if (codec_type == AVMEDIA_TYPE_AUDIO) {
        if (st->codecpar->sample_rate != 0 && st->codecpar->channels != 0) {
          goto the_end;
        }
      } else if (codec_type == AVMEDIA_TYPE_VIDEO || codec_type == AVMEDIA_TYPE_SUBTITLE) {
        goto the_end;
      }
    }
  }
the_end:
  if (p && stream_index != invalid_stream_index) {
    stream_index = p->stream_index[stream_index];
  }
  INFO_LOG() << "Switch " << av_get_media_type_string(static_cast<AVMediaType>(codec_type)) << "  stream from #"
             << old_index << " to #" << stream_index;
  StreamComponentClose(old_index);
  StreamComponentOpen(stream_index);
}

common::Error VideoState::RequestVideo(int width, int height, int av_pixel_format, AVRational aspect_ratio) {
  if (!handler_) {
    return common::Error();
  }

  return handler_->HandleRequestVideo(this, width, height, av_pixel_format, aspect_ratio);
}

int VideoState::SynchronizeAudio(int nb_samples) {
  int wanted_nb_samples = nb_samples;

  /* if not master, then we try to remove or add samples to correct the clock */
  if (GetMasterSyncType() != AV_SYNC_AUDIO_MASTER) {
    clock64_t diff = astream_->GetClock() - GetMasterClock();
    if (IsValidClock(diff) && std::abs(diff) < AV_NOSYNC_THRESHOLD_MSEC) {
      audio_diff_cum_ = diff + audio_diff_avg_coef_ * audio_diff_cum_;
      if (audio_diff_avg_count_ < AUDIO_DIFF_AVG_NB) {
        /* not enough measures to have a correct estimate */
        audio_diff_avg_count_++;
      } else {
        /* estimate the A-V difference */
        double avg_diff = audio_diff_cum_ * (1.0 - audio_diff_avg_coef_);
        if (fabs(avg_diff) >= audio_diff_threshold_) {
          wanted_nb_samples = nb_samples + static_cast<int>(diff * audio_src_.freq);
          int min_nb_samples = ((nb_samples * (100 - SAMPLE_CORRECTION_PERCENT_MAX) / 100));
          int max_nb_samples = ((nb_samples * (100 + SAMPLE_CORRECTION_PERCENT_MAX) / 100));
          wanted_nb_samples = stable_value_in_range(wanted_nb_samples, min_nb_samples, max_nb_samples);
        }
        DEBUG_LOG() << "diff=" << diff << " adiff=" << avg_diff << " sample_diff=" << wanted_nb_samples - nb_samples
                    << " apts=" << audio_clock_ << " " << audio_diff_threshold_;
      }
    } else {
      /* too big difference : may be initial PTS errors, so
         reset A-V filter */
      audio_diff_avg_count_ = 0;
      audio_diff_cum_ = 0;
    }
  }

  return wanted_nb_samples;
}

int VideoState::AudioDecodeFrame() {
  if (paused_) {
    return ERROR_RESULT_VALUE;
  }

  if (!IsAudioReady()) {
    return ERROR_RESULT_VALUE;
  }

  frames::AudioFrame* af = audio_frame_queue_->GetPeekReadable();
  if (!af) {
    return ERROR_RESULT_VALUE;
  }
  audio_frame_queue_->Pop();

  const AVSampleFormat sample_fmt = static_cast<AVSampleFormat>(af->frame->format);
  int data_size = av_samples_get_buffer_size(nullptr, af->frame->channels, af->frame->nb_samples, sample_fmt, 1);
  int64_t dec_channel_layout =
      (af->frame->channel_layout && af->frame->channels == av_get_channel_layout_nb_channels(af->frame->channel_layout))
          ? af->frame->channel_layout
          : av_get_default_channel_layout(af->frame->channels);
  int wanted_nb_samples = SynchronizeAudio(af->frame->nb_samples);

  if (af->frame->format != audio_src_.fmt || dec_channel_layout != audio_src_.channel_layout ||
      af->frame->sample_rate != audio_src_.freq || (wanted_nb_samples != af->frame->nb_samples && !swr_ctx_)) {
    swr_free(&swr_ctx_);
    swr_ctx_ = swr_alloc_set_opts(nullptr, audio_tgt_.channel_layout, audio_tgt_.fmt, audio_tgt_.freq,
                                  dec_channel_layout, sample_fmt, af->frame->sample_rate, 0, nullptr);
    if (!swr_ctx_ || swr_init(swr_ctx_) < 0) {
      ERROR_LOG() << "Cannot create sample rate converter for conversion of " << af->frame->sample_rate << " Hz "
                  << av_get_sample_fmt_name(sample_fmt) << " " << af->frame->channels << " channels to "
                  << audio_tgt_.freq << " Hz " << av_get_sample_fmt_name(audio_tgt_.fmt) << " " << audio_tgt_.channels
                  << " channels!";
      swr_free(&swr_ctx_);
      return ERROR_RESULT_VALUE;
    }
    audio_src_.channel_layout = dec_channel_layout;
    audio_src_.channels = af->frame->channels;
    audio_src_.freq = af->frame->sample_rate;
    audio_src_.fmt = sample_fmt;
  }

  int resampled_data_size = 0;
  if (swr_ctx_) {
    const uint8_t** in = const_cast<const uint8_t**>(af->frame->extended_data);
    uint8_t** out = &audio_buf1_;
    int out_count = wanted_nb_samples * audio_tgt_.freq / af->frame->sample_rate + 256;
    int out_size = av_samples_get_buffer_size(nullptr, audio_tgt_.channels, out_count, audio_tgt_.fmt, 0);
    if (out_size < 0) {
      ERROR_LOG() << "av_samples_get_buffer_size() failed";
      return ERROR_RESULT_VALUE;
    }
    if (wanted_nb_samples != af->frame->nb_samples) {
      if (swr_set_compensation(swr_ctx_,
                               (wanted_nb_samples - af->frame->nb_samples) * audio_tgt_.freq / af->frame->sample_rate,
                               wanted_nb_samples * audio_tgt_.freq / af->frame->sample_rate) < 0) {
        ERROR_LOG() << "swr_set_compensation() failed";
        return ERROR_RESULT_VALUE;
      }
    }
    av_fast_malloc(&audio_buf1_, &audio_buf1_size_, out_size);
    if (!audio_buf1_) {
      return AVERROR(ENOMEM);
    }
    int len2 = swr_convert(swr_ctx_, out, out_count, in, af->frame->nb_samples);
    if (len2 < 0) {
      ERROR_LOG() << "swr_convert() failed";
      return -1;
    }
    if (len2 == out_count) {
      WARNING_LOG() << "audio buffer is probably too small";
      if (swr_init(swr_ctx_) < 0) {
        swr_free(&swr_ctx_);
      }
    }
    audio_buf_ = audio_buf1_;
    resampled_data_size = len2 * audio_tgt_.channels * av_get_bytes_per_sample(audio_tgt_.fmt);
  } else {
    audio_buf_ = af->frame->data[0];
    resampled_data_size = data_size;
  }

  /* update the audio clock with the pts */
  if (IsValidClock(af->pts)) {
    const double div = static_cast<double>(af->frame->nb_samples) / af->frame->sample_rate;
    const clock64_t dur = div * 1000;
    audio_clock_ = af->pts + dur;
  } else {
    audio_clock_ = invalid_clock();
  }
  return resampled_data_size;
}

frames::VideoFrame* VideoState::GetVideoFrame() {
retry:
  if (video_frame_queue_->IsEmpty()) {
    return nullptr;
  }

  /* dequeue the picture */
  frames::VideoFrame* lastvp = video_frame_queue_->PeekLast();
  frames::VideoFrame* firstvp = video_frame_queue_->Peek();

  if (frame_timer_ == 0) {
    frame_timer_ = GetRealClockTime();
  }

  if (paused_) {
    return SelectVideoFrame();
  }

  /* compute nominal last_duration */
  clock64_t last_duration = CalcDurationBetweenVideoFrames(lastvp, firstvp, max_frame_duration_);
  clock64_t delay = ComputeTargetDelay(last_duration);
  clock64_t time = GetRealClockTime();
  clock64_t next_frame_ts = frame_timer_ + delay;
  if (time < next_frame_ts) {
    return SelectVideoFrame();
  }

  frame_timer_ = next_frame_ts;
  if (delay > 0) {
    if (time - frame_timer_ > AV_SYNC_THRESHOLD_MAX_MSEC) {
      frame_timer_ = time;
    }
  }

  const clock64_t pts = firstvp->pts;
  if (IsValidClock(pts)) {
    /* update current video pts */
    vstream_->SetClockAt(pts, time);
  }

  frames::VideoFrame* nextvp = video_frame_queue_->PeekNextOrNull();
  if (nextvp) {
    clock64_t duration = CalcDurationBetweenVideoFrames(firstvp, nextvp, max_frame_duration_);
    if ((opt_.framedrop == FRAME_DROP_AUTO ||
         (opt_.framedrop == FRAME_DROP_ON || (GetMasterSyncType() != AV_SYNC_VIDEO_MASTER)))) {
      clock64_t next_next_frame_ts = frame_timer_ + duration;
      clock64_t diff_drop = time - next_next_frame_ts;
      if (diff_drop > 0) {
        stats_->frame_drops_late++;
        video_frame_queue_->Pop();
        goto retry;
      }
    }
  }

  video_frame_queue_->Pop();
  force_refresh_ = true;
  if (step_ && !paused_) {
    StreamTogglePause();
  }
  return SelectVideoFrame();
}

frames::VideoFrame* VideoState::SelectVideoFrame() const {
  if (force_refresh_ && video_frame_queue_->RindexShown()) {
    frames::VideoFrame* vp = video_frame_queue_->PeekLast();
    if (vp) {
      stats_->frame_processed++;
      return vp;
    }

    return nullptr;
  }

  return nullptr;
}

frames::VideoFrame* VideoState::TryToGetVideoFrame() {
  if (paused_ && !force_refresh_) {  // if in pause and not force update
    return nullptr;
  }

  const stream_format_t fmt = GetStreamFormat();

  int aqsize = 0, vqsize = 0;
  common::media::bandwidth_t video_bandwidth = 0, audio_bandwidth = 0;
  if (fmt & HAVE_VIDEO_STREAM) {
    PacketQueue* video_packet_queue = vstream_->GetQueue();
    vqsize = video_packet_queue->GetSize();
    video_bandwidth = vstream_->Bandwidth();
  }
  if (fmt & HAVE_AUDIO_STREAM) {
    PacketQueue* audio_packet_queue = astream_->GetQueue();
    aqsize = audio_packet_queue->GetSize();
    audio_bandwidth = astream_->Bandwidth();
  }

  stats_->master_pts = GetMasterPts();
  stats_->master_clock = GetMasterClock();
  stats_->video_clock = vstream_->GetClock();
  stats_->audio_clock = astream_->GetClock();
  stats_->fmt = fmt;
  stats_->audio_queue_size = aqsize;
  stats_->video_queue_size = vqsize;
  stats_->audio_bandwidth = audio_bandwidth;
  stats_->video_bandwidth = video_bandwidth;
  stats_->active_hwaccel = static_cast<HWDeviceType>(input_st_->active_hwaccel_id);

  if (fmt & HAVE_VIDEO_STREAM && video_frame_queue_) {
    frames::VideoFrame* fr = GetVideoFrame();
    force_refresh_ = false;
    return fr;
  }

  return nullptr;
}

VideoState::stats_t VideoState::GetStatistic() const {
  return stats_;
}

AVRational VideoState::GetFrameRate() const {
  return vstream_->GetFrameRate();
}

void VideoState::UpdateAudioBuffer(uint8_t* stream, int len, int audio_volume) {
  if (!IsStreamReady()) {
    return;
  }

  const clock64_t audio_callback_time = GetRealClockTime();
  while (len > 0) {
    if (audio_buf_index_ >= audio_buf_size_) {
      int audio_size = AudioDecodeFrame();
      if (audio_size < 0) {
        /* if error, just output silence */
        audio_buf_ = nullptr;
        audio_buf_size_ = AUDIO_MIN_BUFFER_SIZE / audio_tgt_.frame_size * audio_tgt_.frame_size;
      } else {
        audio_buf_size_ = audio_size;
      }
      audio_buf_index_ = 0;
    }
    int len1 = audio_buf_size_ - audio_buf_index_;
    if (len1 > len) {
      len1 = len;
    }
    if (audio_buf_ && audio_volume == 100) {
      memcpy(stream, audio_buf_ + audio_buf_index_, len1);
    } else {
      memset(stream, 0, len1);
      if (audio_buf_) {
        if (handler_) {
          handler_->HanleAudioMix(stream, audio_buf_ + audio_buf_index_, len1, audio_volume);
        }
      }
    }
    len -= len1;
    stream += len1;
    audio_buf_index_ += len1;
  }
  audio_write_buf_size_ = audio_buf_size_ - audio_buf_index_;
  /* Let's assume the audio driver that is used by SDL has two periods. */
  if (IsValidClock(audio_clock_)) {
    double clc = static_cast<double>(2 * audio_hw_buf_size_ + audio_write_buf_size_) /
                 static_cast<double>(audio_tgt_.bytes_per_sec) * 1000;
    const clock64_t pts = audio_clock_ - clc;
    astream_->SetClockAt(pts, audio_callback_time);
  }
}

int VideoState::QueuePicture(AVFrame* src_frame, clock64_t pts, clock64_t duration, int64_t pos) {
  frames::VideoFrame* vp = video_frame_queue_->GetPeekWritable();
  if (!vp) {
    return ERROR_RESULT_VALUE;
  }

  vp->sar = src_frame->sample_aspect_ratio;

  /* alloc or resize hardware picture buffer */
  if (vp->width != src_frame->width || vp->height != src_frame->height || vp->format != src_frame->format) {
    vp->width = src_frame->width;
    vp->height = src_frame->height;
    vp->format = static_cast<AVPixelFormat>(src_frame->format);
    if (handler_) {
      handler_->HandleFrameResize(this, vp->width, vp->height, vp->format, vp->sar);
    }
  }

  /* if the frame is not skipped, then display it */
  vp->pts = pts;
  vp->duration = duration;
  vp->pos = pos;

  av_frame_move_ref(vp->frame, src_frame);
  video_frame_queue_->Push();
  return SUCCESS_RESULT_VALUE;
}

int VideoState::GetVideoFrame(AVFrame* frame) {
  int got_picture = viddec_->DecodeFrame(frame);
  if (got_picture < 0) {
    return ERROR_RESULT_VALUE;
  }

  if (got_picture) {
    frame->sample_aspect_ratio = vstream_->StableAspectRatio(frame);

    if (opt_.framedrop == FRAME_DROP_AUTO || (opt_.framedrop || GetMasterSyncType() != AV_SYNC_VIDEO_MASTER)) {
      if (IsValidPts(frame->pts)) {
        clock64_t dpts = vstream_->q2d() * frame->pts;
        clock64_t diff = dpts - GetMasterClock();
        PacketQueue* video_packet_queue = vstream_->GetQueue();
        if (IsValidClock(diff) && std::abs(diff) < AV_NOSYNC_THRESHOLD_MSEC && diff - frame_last_filter_delay_ < 0 &&
            video_packet_queue->GetNbPackets()) {
          stats_->frame_drops_early++;
          av_frame_unref(frame);
          got_picture = 0;
        }
      }
    }
  }

  return got_picture;
}

/* this thread gets the stream from the disk or the network */
int VideoState::ReadRoutine() {
  AVFormatContext* ic = avformat_alloc_context();
  if (!ic) {
    const int av_errno = AVERROR(ENOMEM);
    common::ErrnoError err = common::make_errno_error(AVUNERROR(av_errno));
    if (handler_) {
      handler_->HandleQuitStream(this, av_errno, common::make_error_from_errno(err));
    }
    return ERROR_RESULT_VALUE;
  }

  bool scan_all_pmts_set = false;
  const std::string uri_str = make_url(uri_);
  if (uri_str.empty()) {
    common::Error err = common::make_error_inval();
    if (handler_) {
      handler_->HandleQuitStream(this, EINVAL, err);
    }
    return ERROR_RESULT_VALUE;
  }

  const char* in_filename = uri_str.c_str();
  ic->interrupt_callback.callback = decode_interrupt_callback;
  ic->interrupt_callback.opaque = this;
  if (!av_dict_get(copt_.format_opts, "scan_all_pmts", nullptr, AV_DICT_MATCH_CASE)) {
    av_dict_set(&copt_.format_opts, "scan_all_pmts", "1", AV_DICT_DONT_OVERWRITE);
    scan_all_pmts_set = true;
  }

  int open_result = avformat_open_input(&ic, in_filename, nullptr, &copt_.format_opts);  // autodetect format
  if (open_result < 0) {
    std::string err_str = ffmpeg_errno_to_string(open_result);
    common::Error err = common::make_error(err_str);
    avformat_close_input(&ic);
    if (handler_) {
      handler_->HandleQuitStream(this, open_result, err);
    }
    return ERROR_RESULT_VALUE;
  }
  if (scan_all_pmts_set) {
    av_dict_set(&copt_.format_opts, "scan_all_pmts", nullptr, AV_DICT_MATCH_CASE);
  }

  ic_ = ic;

  VideoStream* video_stream = vstream_;
  AudioStream* audio_stream = astream_;
  PacketQueue* video_packet_queue = video_stream->GetQueue();
  PacketQueue* audio_packet_queue = audio_stream->GetQueue();
  int st_index[AVMEDIA_TYPE_NB];
  memset(st_index, -1, sizeof(st_index));

  if (opt_.genpts) {
    ic->flags |= AVFMT_FLAG_GENPTS;
  }

  av_format_inject_global_side_data(ic);

  AVDictionary** opts = setup_find_stream_info_opts(ic, copt_.codec_opts);
  unsigned int orig_nb_streams = ic->nb_streams;

  int find_stream_info_result = avformat_find_stream_info(ic, opts);

  for (unsigned int i = 0; i < orig_nb_streams; i++) {
    av_dict_free(&opts[i]);
  }
  av_freep(&opts);

  AVPacket pkt1, *pkt = &pkt1;
  if (find_stream_info_result < 0) {
    std::string err_str = ffmpeg_errno_to_string(find_stream_info_result);
    common::Error err = common::make_error(err_str);
    if (handler_) {
      handler_->HandleQuitStream(this, -1, err);
    }
    return ERROR_RESULT_VALUE;
  }

  if (ic->pb) {
    ic->pb->eof_reached = 0;  // FIXME hack, ffplay maybe should not use
                              // avio_feof() to test for the end
  }

  max_frame_duration_ = (ic->iformat->flags & AVFMT_TS_DISCONT) ? 10000 : 3600000;

  if (opt_.seek_by_bytes == SEEK_AUTO) {
    bool seek = (ic->iformat->flags & AVFMT_TS_DISCONT) && strcmp("ogg", ic->iformat->name);
    opt_.seek_by_bytes = seek ? SEEK_BY_BYTES_ON : SEEK_BY_BYTES_OFF;
  }

  realtime_ = is_realtime(ic);

  av_dump_format(ic, 0, id_.c_str(), 0);

  for (int i = 0; i < static_cast<int>(ic->nb_streams); i++) {
    AVStream* st = ic->streams[i];
    enum AVMediaType type = st->codecpar->codec_type;
    st->discard = AVDISCARD_ALL;
    if (type >= 0 && !opt_.wanted_stream_spec[type].empty() && st_index[type] == -1) {
      const char* want_spec = opt_.wanted_stream_spec[type].c_str();
      if (avformat_match_stream_specifier(ic, st, want_spec) > 0) {
        st_index[type] = i;
      }
    }
  }
  for (int i = 0; i < static_cast<int>(AVMEDIA_TYPE_NB); i++) {
    if (!opt_.wanted_stream_spec[i].empty() && st_index[i] == -1) {
      ERROR_LOG() << "Stream specifier " << opt_.wanted_stream_spec[i] << " does not match any "
                  << av_get_media_type_string(static_cast<AVMediaType>(i)) << " stream";
      st_index[i] = INT_MAX;
    }
  }

  st_index[AVMEDIA_TYPE_VIDEO] =
      av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO, st_index[AVMEDIA_TYPE_VIDEO], -1, nullptr, 0);

  st_index[AVMEDIA_TYPE_AUDIO] = av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO, st_index[AVMEDIA_TYPE_AUDIO],
                                                     st_index[AVMEDIA_TYPE_VIDEO], nullptr, 0);

  /* open the streams */
  if (st_index[AVMEDIA_TYPE_AUDIO] >= 0) {
    int res_audio = StreamComponentOpen(st_index[AVMEDIA_TYPE_AUDIO]);
    if (res_audio < 0) {
      WARNING_LOG() << "Failed to open audio stream";
    }
  }

  if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
    int res_video = StreamComponentOpen(st_index[AVMEDIA_TYPE_VIDEO]);
    if (res_video < 0) {
      WARNING_LOG() << "Failed to open video stream";
    }
  }

  const stream_format_t fmt = GetStreamFormat();
  common::media::DesireBytesPerSec video_bandwidth_calc = video_stream->DesireBandwith();
  if (fmt & HAVE_VIDEO_STREAM) {
    if (!video_bandwidth_calc.IsValid()) {
      WARNING_LOG() << "Can't calculate video bandwidth.";
    }
  }
  common::media::DesireBytesPerSec audio_bandwidth_calc = audio_stream->DesireBandwith();
  if (fmt & HAVE_AUDIO_STREAM) {
    if (!audio_bandwidth_calc.IsValid()) {
      WARNING_LOG() << "Can't calculate audio bandwidth.";
    }
  }

  if (fmt == (HAVE_VIDEO_STREAM | HAVE_AUDIO_STREAM)) {
  } else if (fmt == HAVE_VIDEO_STREAM) {
    opt_.av_sync_type = AV_SYNC_VIDEO_MASTER;
  } else if (fmt == HAVE_AUDIO_STREAM) {
    opt_.av_sync_type = AV_SYNC_AUDIO_MASTER;
  } else {
    DNOTREACHED();
  }

  const common::media::DesireBytesPerSec band = video_bandwidth_calc + audio_bandwidth_calc;
  if (ic_->bit_rate) {
    common::media::bandwidth_t bytes_per_sec = ic_->bit_rate / 8;
    if (!band.InRange(bytes_per_sec)) {
      WARNING_LOG() << "Stream bitrate is: " << bytes_per_sec << " not in our calculation range(" << band.min << "/"
                    << band.max << ").";
    }
  }
  if (opt_.infinite_buffer < 0 && realtime_) {
    opt_.infinite_buffer = 1;
  }

  ResetStats();
  while (!IsAborted()) {
    if (paused_ != last_paused_) {
      last_paused_ = paused_;
      if (paused_) {
        read_pause_return_ = av_read_pause(ic);
      } else {
        av_read_play(ic);
        ResetStats();
      }
    }

    if (seek_req_) {
      int64_t seek_target = seek_pos_;
      int64_t seek_min = seek_rel_ > 0 ? seek_target - seek_rel_ + 2 : INT64_MIN;
      int64_t seek_max = seek_rel_ < 0 ? seek_target - seek_rel_ - 2 : INT64_MAX;
      // FIXME the +-2 is due to rounding being not done in the correct
      // direction in generation
      //      of the seek_pos/seek_rel variables

      int ret = avformat_seek_file(ic, -1, seek_min, seek_target, seek_max, seek_flags_);
      if (ret < 0) {
        ERROR_LOG() << "Seeking " << id_ << "failed error: " << ffmpeg_errno_to_string(ret);
      } else {
        if (video_stream->IsOpened()) {
          video_packet_queue->PutNullpacket(video_stream->Index());
        }
        if (audio_stream->IsOpened()) {
          audio_packet_queue->PutNullpacket(audio_stream->Index());
        }
      }
      seek_req_ = false;
      eof_ = false;
      if (paused_) {
        StepToNextFrame();
      }
      ResetStats();
    }

    /* if the queue are full, no need to read more */
    if (opt_.infinite_buffer < 1 && (video_packet_queue->GetSize() + audio_packet_queue->GetSize() > MAX_QUEUE_SIZE ||
                                     (astream_->HasEnoughPackets() && vstream_->HasEnoughPackets()))) {
      std::unique_lock<std::mutex> lock(read_thread_mutex_);
      std::cv_status interrupt_status = read_thread_cond_.wait_for(lock, std::chrono::milliseconds(10));
      if (interrupt_status == std::cv_status::no_timeout) {  // if notify
      }
      continue;
    }
    if (!paused_ && eof_) {
      bool is_audio_dec_ready = auddec_ && audio_frame_queue_;
      bool is_audio_not_finished_but_empty = false;
      if (is_audio_dec_ready) {
        is_audio_not_finished_but_empty = !auddec_->IsFinished() && audio_frame_queue_->IsEmpty();
      }
      bool is_video_dec_ready = viddec_ && video_frame_queue_;
      bool is_video_not_finished_but_empty = false;
      if (is_video_dec_ready) {
        is_video_not_finished_but_empty = !viddec_->IsFinished() && video_frame_queue_->IsEmpty();
      }
      if ((!is_audio_dec_ready || (is_audio_dec_ready && is_audio_not_finished_but_empty)) &&
          (!is_video_dec_ready || (is_video_dec_ready && is_video_not_finished_but_empty)) && opt_.auto_exit) {
        INFO_LOG() << "EOF is_audio_dec_ready: " << is_audio_dec_ready
                   << ", is_video_dec_ready: " << is_video_dec_ready;
        int errn = AVERROR_EOF;
        std::string err_str = ffmpeg_errno_to_string(errn);
        common::Error err = common::make_error(err_str);
        if (handler_) {
          handler_->HandleQuitStream(this, errn, err);
        }
        return ERROR_RESULT_VALUE;
      }
    }
    int ret = av_read_frame(ic, pkt);
    if (ret < 0) {
      WARNING_LOG() << "Read input stream error: " << ffmpeg_errno_to_string(ret);
      bool is_eof = ret == AVERROR_EOF;
      bool is_feof = avio_feof(ic->pb);
      if ((is_eof || is_feof) && !eof_) {
        if (video_stream->IsOpened()) {
          video_packet_queue->PutNullpacket(video_stream->Index());
        }
        if (audio_stream->IsOpened()) {
          audio_packet_queue->PutNullpacket(audio_stream->Index());
        }
        eof_ = true;
      }
      if (ic->pb && ic->pb->error) {
        break;
      }
      continue;
    } else {
      eof_ = false;
    }

    if (pkt->stream_index == audio_stream->Index()) {
      audio_stream->RegisterPacket(pkt);
      audio_packet_queue->Put(pkt);
    } else if (pkt->stream_index == video_stream->Index()) {
      if (video_stream->HaveDispositionPicture()) {
        av_packet_unref(pkt);
      } else {
        video_stream->RegisterPacket(pkt);
        video_packet_queue->Put(pkt);
      }
    } else {
      av_packet_unref(pkt);
    }
  }

  if (handler_) {
    handler_->HandleQuitStream(this, 0, common::Error());
  }
  return SUCCESS_RESULT_VALUE;
}

stream_format_t VideoState::GetStreamFormat() const {
  const bool is_video_open = vstream_->IsOpened();
  const bool is_audio_open = astream_->IsOpened();
  return (is_audio_open && is_video_open)
             ? (HAVE_VIDEO_STREAM | HAVE_AUDIO_STREAM)
             : (is_video_open ? HAVE_VIDEO_STREAM : (is_audio_open ? HAVE_AUDIO_STREAM : UNKNOWN_STREAM));
}

int VideoState::decode_interrupt_callback(void* user_data) {
  VideoState* is = static_cast<VideoState*>(user_data);
  if (is->IsAborted()) {
    return 1;
  }

  return 0;
}

int VideoState::AudioThread() {
  frames::AudioFrame* af = nullptr;
  int ret = 0;

  AVFrame* frame = av_frame_alloc();
  if (!frame) {
    return AVERROR(ENOMEM);
  }

  do {
    int got_frame = auddec_->DecodeFrame(frame);
    if (got_frame < 0) {
      break;
    }

    if (got_frame) {
      AVRational tb = {1, frame->sample_rate};

#if CONFIG_AVFILTER
      int64_t dec_channel_layout = get_valid_channel_layout(frame->channel_layout, frame->channels);

      bool reconfigure = cmp_audio_fmts(audio_filter_src_.fmt, audio_filter_src_.channels,
                                        static_cast<AVSampleFormat>(frame->format), frame->channels) ||
                         audio_filter_src_.channel_layout != dec_channel_layout ||
                         audio_filter_src_.freq != frame->sample_rate;

      if (reconfigure) {
        char buf1[1024], buf2[1024];
        av_get_channel_layout_string(buf1, SIZEOFMASS(buf1), -1, audio_filter_src_.channel_layout);
        av_get_channel_layout_string(buf2, SIZEOFMASS(buf2), -1, dec_channel_layout);
        const std::string mess = common::MemSPrintf(
            "Audio frame changed from rate:%d ch:%d fmt:%s layout:%s serial:%d "
            "to rate:%d ch:%d "
            "fmt:%s layout:%s serial:%d\n",
            audio_filter_src_.freq, audio_filter_src_.channels, av_get_sample_fmt_name(audio_filter_src_.fmt), buf1, 0,
            frame->sample_rate, frame->channels, av_get_sample_fmt_name(static_cast<AVSampleFormat>(frame->format)),
            buf2, 0);
        DEBUG_LOG() << mess;

        audio_filter_src_.fmt = static_cast<AVSampleFormat>(frame->format);
        audio_filter_src_.channels = frame->channels;
        audio_filter_src_.channel_layout = dec_channel_layout;
        audio_filter_src_.freq = frame->sample_rate;

        if ((ret = ConfigureAudioFilters(opt_.afilters, 1)) < 0) {
          break;
        }
      }

      ret = av_buffersrc_add_frame(in_audio_filter_, frame);
      if (ret < 0) {
        break;
      }

      while ((ret = av_buffersink_get_frame_flags(out_audio_filter_, frame, 0)) >= 0) {
        tb = out_audio_filter_->inputs[0]->time_base;
#endif
        af = audio_frame_queue_->GetPeekWritable();
        if (!af) {  // if stoped
#if CONFIG_AVFILTER
          avfilter_graph_free(&agraph_);
#endif
          av_frame_free(&frame);
          return ret;
        }

        af->pts = IsValidPts(frame->pts) ? frame->pts * q2d_diff(tb) : invalid_clock();
        af->pos = frame->pkt_pos;
        af->format = static_cast<AVSampleFormat>(frame->format);
        AVRational tmp = {frame->nb_samples, frame->sample_rate};
        af->duration = q2d_diff(tmp);

        av_frame_move_ref(af->frame, frame);
        audio_frame_queue_->Push();

#if CONFIG_AVFILTER
      }
      if (ret == AVERROR_EOF) {
        auddec_->SetFinished(true);
      }
#endif
    }
  } while (ret >= 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF);

#if CONFIG_AVFILTER
  avfilter_graph_free(&agraph_);
#endif
  av_frame_free(&frame);
  return ret;
}

int VideoState::VideoThread() {
  AVFrame* frame = av_frame_alloc();
  if (!frame) {
    return AVERROR(ENOMEM);
  }

#if CONFIG_AVFILTER
  AVFilterGraph* graph = avfilter_graph_alloc();
  AVFilterContext *filt_out = nullptr, *filt_in = nullptr;
  int last_w = 0;
  int last_h = 0;
  enum AVPixelFormat last_format = AV_PIX_FMT_NONE;  // -2
  if (!graph) {
    av_frame_free(&frame);
    return AVERROR(ENOMEM);
  }
#endif

  AVRational tb = vstream_->GetTimeBase();
  AVRational frame_rate = vstream_->GetFrameRate();
  while (true) {
    int ret = GetVideoFrame(frame);
    if (ret < 0) {
      goto the_end;
    }
    if (!ret) {
      continue;
    }

    if (input_st_->hwaccel_retrieve_data && frame->format == input_st_->hwaccel_pix_fmt) {
      int err = input_st_->hwaccel_retrieve_data(viddec_->GetAvCtx(), frame);
      if (err < 0) {
        continue;
      }
    }
    input_st_->hwaccel_retrieved_pix_fmt = static_cast<AVPixelFormat>(frame->format);

#if CONFIG_AVFILTER
    if (last_w != frame->width || last_h != frame->height || last_format != frame->format) {  // -vf option
      const std::string mess = common::MemSPrintf(
          "Video frame changed from size:%dx%d format:%s serial:%d to "
          "size:%dx%d format:%s "
          "serial:%d",
          last_w, last_h, static_cast<const char*>(av_x_if_null(av_get_pix_fmt_name(last_format), "none")), 0,
          frame->width, frame->height,
          static_cast<const char*>(
              av_x_if_null(av_get_pix_fmt_name(static_cast<AVPixelFormat>(frame->format)), "none")),
          0);
      DEBUG_LOG() << mess;
      avfilter_graph_free(&graph);
      graph = avfilter_graph_alloc();
      const std::string vfilters = opt_.vfilters;
      int ret = ConfigureVideoFilters(graph, vfilters, frame);
      if (ret < 0) {
        ERROR_LOG() << "Internal video error!";
        goto the_end;
      }
      filt_in = in_video_filter_;
      filt_out = out_video_filter_;
      last_w = frame->width;
      last_h = frame->height;
      last_format = static_cast<AVPixelFormat>(frame->format);
      frame_rate = filt_out->inputs[0]->frame_rate;
    }

    ret = av_buffersrc_add_frame(filt_in, frame);
    if (ret < 0) {
      goto the_end;
    }

    while (ret >= 0) {
      frame_last_returned_time_ = GetRealClockTime();

      ret = av_buffersink_get_frame_flags(filt_out, frame, 0);
      if (ret < 0) {
        if (ret == AVERROR_EOF) {
          viddec_->SetFinished(true);
        }
        ret = 0;
        break;
      }

      frame_last_filter_delay_ = GetRealClockTime() - frame_last_returned_time_;
      if (std::abs(frame_last_filter_delay_) > AV_NOSYNC_THRESHOLD_MSEC) {
        frame_last_filter_delay_ = 0;
      }
      if (filt_out) {
        tb = filt_out->inputs[0]->time_base;
      }
#endif
      AVRational fr = {frame_rate.den, frame_rate.num};
      clock64_t duration = (frame_rate.num && frame_rate.den ? q2d_diff(fr) : 0);
      clock64_t pts = IsValidPts(frame->pts) ? frame->pts * q2d_diff(tb) : invalid_clock();
      ret = QueuePicture(frame, pts, duration, frame->pkt_pos);
      av_frame_unref(frame);
#if CONFIG_AVFILTER
    }
#endif

    if (ret < 0) {
      goto the_end;
    }
  }
the_end:
#if CONFIG_AVFILTER
  avfilter_graph_free(&graph);
#endif
  av_frame_free(&frame);
  return 0;
}

#if CONFIG_AVFILTER
int VideoState::ConfigureVideoFilters(AVFilterGraph* graph, const std::string& vfilters, AVFrame* frame) {
  static const enum AVPixelFormat pix_fmts[] = {AV_PIX_FMT_YUV420P, AV_PIX_FMT_BGRA, AV_PIX_FMT_NONE};
  AVDictionary* sws_dict = copt_.sws_dict;
  AVDictionaryEntry* e = nullptr;
  char sws_flags_str[512] = {0};
  while ((e = av_dict_get(sws_dict, "", e, AV_DICT_IGNORE_SUFFIX))) {
    if (strcmp(e->key, "sws_flags") == 0) {
      av_strlcatf(sws_flags_str, sizeof(sws_flags_str), "%s=%s:", "flags", e->value);
    } else {
      av_strlcatf(sws_flags_str, sizeof(sws_flags_str), "%s=%s:", e->key, e->value);
    }
  }
  const size_t len_sws_flags = strlen(sws_flags_str);
  if (len_sws_flags) {
    sws_flags_str[len_sws_flags - 1] = 0;
  }

  AVCodecParameters* codecpar = vstream_->GetCodecpar();
  if (!codecpar) {
    DNOTREACHED();
    return ERROR_RESULT_VALUE;
  }

  AVRational tb = vstream_->GetTimeBase();
  char buffersrc_args[256];
  AVFilterContext *filt_src = nullptr, *filt_out = nullptr, *last_filter = nullptr;
  graph->scale_sws_opts = av_strdup(sws_flags_str);
  snprintf(buffersrc_args, sizeof(buffersrc_args), "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
           frame->width, frame->height, frame->format, tb.num, tb.den, codecpar->sample_aspect_ratio.num,
           FFMAX(codecpar->sample_aspect_ratio.den, 1));
  AVRational fr = vstream_->GetFrameRate();
  if (fr.num && fr.den) {
    av_strlcatf(buffersrc_args, sizeof(buffersrc_args), ":frame_rate=%d/%d", fr.num, fr.den);
  }

  int ret = avfilter_graph_create_filter(&filt_src, avfilter_get_by_name("buffer"), "ffplay_buffer", buffersrc_args,
                                         nullptr, graph);
  if (ret < 0) {
    return ret;
  }

  ret = avfilter_graph_create_filter(&filt_out, avfilter_get_by_name("buffersink"), "ffplay_buffersink", nullptr,
                                     nullptr, graph);
  if (ret < 0) {
    return ret;
  }
  ret = av_opt_set_int_list(filt_out, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
  if (ret < 0) {
    WARNING_LOG() << "Failed to set pix_fmts ret: " << ret;
    return ret;
  }

  last_filter = filt_out;

/* Note: this macro adds a filter before the lastly added filter, so the
 * processing order of the filters is in reverse */
#define INSERT_FILT(name, arg)                                                                                      \
  do {                                                                                                              \
    AVFilterContext* filt_ctx;                                                                                      \
                                                                                                                    \
    ret = avfilter_graph_create_filter(&filt_ctx, avfilter_get_by_name(name), "ffplay_" name, arg, nullptr, graph); \
    if (ret < 0) {                                                                                                  \
      return ret;                                                                                                   \
    }                                                                                                               \
    ret = avfilter_link(filt_ctx, 0, last_filter, 0);                                                               \
    if (ret < 0) {                                                                                                  \
      return ret;                                                                                                   \
    }                                                                                                               \
    last_filter = filt_ctx;                                                                                         \
  } while (0)

  if (opt_.autorotate) {
    double theta = vstream_->GetRotation();

    if (fabs(theta - 90) < 1.0) {
      INSERT_FILT("transpose", "clock");
    } else if (fabs(theta - 180) < 1.0) {
      INSERT_FILT("hflip", nullptr);
      INSERT_FILT("vflip", nullptr);
    } else if (fabs(theta - 270) < 1.0) {
      INSERT_FILT("transpose", "cclock");
    } else if (fabs(theta) > 1.0) {
      char rotate_buf[64];
      snprintf(rotate_buf, sizeof(rotate_buf), "%f*PI/180", theta);
      INSERT_FILT("rotate", rotate_buf);
    }
  }

  const char* vfilters_ptr = vfilters.empty() ? nullptr : vfilters.c_str();
  ret = configure_filtergraph(graph, vfilters_ptr, filt_src, last_filter);
  if (ret < 0) {
    WARNING_LOG() << "Failed to configure_filtergraph ret: " << ret;
    return ret;
  }

  in_video_filter_ = filt_src;
  out_video_filter_ = filt_out;
  return ret;
}

int VideoState::ConfigureAudioFilters(const std::string& afilters, int force_output_format) {
  static const enum AVSampleFormat sample_fmts[] = {AV_SAMPLE_FMT_S16, AV_SAMPLE_FMT_NONE};
  avfilter_graph_free(&agraph_);
  agraph_ = avfilter_graph_alloc();
  if (!agraph_) {
    return AVERROR(ENOMEM);
  }

  AVDictionaryEntry* e = nullptr;
  char aresample_swr_opts[512] = "";
  AVDictionary* swr_opts = copt_.swr_opts;
  while ((e = av_dict_get(swr_opts, "", e, AV_DICT_IGNORE_SUFFIX))) {
    av_strlcatf(aresample_swr_opts, sizeof(aresample_swr_opts), "%s=%s:", e->key, e->value);
  }
  size_t len = strlen(aresample_swr_opts);
  if (len) {
    aresample_swr_opts[len - 1] = '\0';
  }
  av_opt_set(agraph_, "aresample_swr_opts", aresample_swr_opts, 0);

  char asrc_args[256];
  int ret = snprintf(asrc_args, sizeof(asrc_args), "sample_rate=%d:sample_fmt=%s:channels=%d:time_base=%d/%d",
                     audio_filter_src_.freq, av_get_sample_fmt_name(audio_filter_src_.fmt), audio_filter_src_.channels,
                     1, audio_filter_src_.freq);
  if (audio_filter_src_.channel_layout) {
    snprintf(asrc_args + ret, sizeof(asrc_args) - ret, ":channel_layout=0x%" PRIx64, audio_filter_src_.channel_layout);
  }

  AVFilterContext *filt_asrc = nullptr, *filt_asink = nullptr;
  ret = avfilter_graph_create_filter(&filt_asrc, avfilter_get_by_name("abuffer"), "ffplay_abuffer", asrc_args, nullptr,
                                     agraph_);
  if (ret < 0) {
    avfilter_graph_free(&agraph_);
    return ret;
  }

  ret = avfilter_graph_create_filter(&filt_asink, avfilter_get_by_name("abuffersink"), "ffplay_abuffersink", nullptr,
                                     nullptr, agraph_);
  if (ret < 0) {
    avfilter_graph_free(&agraph_);
    return ret;
  }

  ret = av_opt_set_int_list(filt_asink, "sample_fmts", sample_fmts, AV_SAMPLE_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
  if (ret < 0) {
    avfilter_graph_free(&agraph_);
    return ret;
  }
  ret = av_opt_set_int(filt_asink, "all_channel_counts", 1, AV_OPT_SEARCH_CHILDREN);
  if (ret < 0) {
    avfilter_graph_free(&agraph_);
    return ret;
  }

  if (force_output_format) {
    int channels[2] = {0, -1};
    int64_t channel_layouts[2] = {0, -1};
    int sample_rates[2] = {0, -1};
    channel_layouts[0] = audio_tgt_.channel_layout;
    channels[0] = audio_tgt_.channels;
    sample_rates[0] = audio_tgt_.freq;
    ret = av_opt_set_int(filt_asink, "all_channel_counts", 0, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
      avfilter_graph_free(&agraph_);
      return ret;
    }
    ret = av_opt_set_int_list(filt_asink, "channel_layouts", channel_layouts, -1, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
      avfilter_graph_free(&agraph_);
      return ret;
    }
    ret = av_opt_set_int_list(filt_asink, "channel_counts", channels, -1, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
      avfilter_graph_free(&agraph_);
      return ret;
    }
    ret = av_opt_set_int_list(filt_asink, "sample_rates", sample_rates, -1, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
      avfilter_graph_free(&agraph_);
      return ret;
    }
  }

  const char* afilters_ptr = afilters.empty() ? nullptr : afilters.c_str();
  ret = configure_filtergraph(agraph_, afilters_ptr, filt_asrc, filt_asink);
  if (ret < 0) {
    avfilter_graph_free(&agraph_);
    return ret;
  }

  in_audio_filter_ = filt_asrc;
  out_audio_filter_ = filt_asink;
  return ret;
}
#endif /* CONFIG_AVFILTER */

}  // namespace media
}  // namespace fastoplayer
