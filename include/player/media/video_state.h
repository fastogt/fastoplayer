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

#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>

#include <player/media/ffmpeg_config.h>  // for CONFIG_AVFILTER

extern "C" {
#include <libavfilter/avfilter.h>  // for AVFilterContext, AVFilterGraph
#include <libavformat/avformat.h>  // for AVFormatContext
#include <libavutil/avutil.h>      // for AVMediaType
#include <libavutil/frame.h>       // for AVFrame
}

#include <common/error.h>
#include <common/threads/types.h>  // for condition_variable, mutex
#include <common/uri/gurl.h>       // for Uri

#include <player/media/app_options.h>   // for AppOptions, ComplexOptions
#include <player/media/audio_params.h>  // for AudioParams
#include <player/media/stream_statistic.h>
#include <player/media/types.h>  // for clock64_t, AvSyncType

struct SwrContext;
struct InputStream;

namespace common {
namespace threads {
template <typename RT>
class Thread;
}
}  // namespace common

/* no AV correction is done if too big error */
#define VIDEO_PICTURE_QUEUE_SIZE 3
#define SAMPLE_QUEUE_SIZE 9

namespace fastoplayer {
namespace media {

class VideoStateHandler;

class AudioDecoder;
class AudioStream;
class VideoDecoder;
class VideoStream;

namespace frames {
struct AudioFrame;
struct VideoFrame;
template <size_t buffer_size>
class AudioFrameQueue;
template <size_t buffer_size>
class VideoFrameQueue;
}  // namespace frames

class VideoState {
 public:
  typedef std::shared_ptr<Stats> stats_t;
  typedef frames::VideoFrameQueue<VIDEO_PICTURE_QUEUE_SIZE> video_frame_queue_t;
  typedef frames::AudioFrameQueue<SAMPLE_QUEUE_SIZE> audio_frame_queue_t;

  enum { invalid_stream_index = -1 };
  VideoState(stream_id id, const common::uri::GURL& uri, const AppOptions& opt, const ComplexOptions& copt);
  void SetHandler(VideoStateHandler* handler);

  int Exec() WARN_UNUSED_RESULT;
  void Abort();

  bool IsVideoThread() const;
  bool IsAudioThread() const;

  bool IsAborted() const;
  bool IsStreamReady() const;
  stream_id GetId() const;
  const common::uri::GURL& GetUri() const;
  virtual ~VideoState();

  void RefreshRequest();
  /* pause or resume the video */
  void TogglePause();
  bool IsPaused() const;

  void StepToNextFrame();
  void SeekNextChunk();
  void SeekPrevChunk();
  void SeekChapter(int incr);
  void Seek(clock64_t msec);
  void SeekMsec(clock64_t msec);
  void StreamCycleChannel(AVMediaType codec_type);

  common::Error RequestVideo(int width, int height, int av_pixel_format, AVRational aspect_ratio) WARN_UNUSED_RESULT;

  frames::VideoFrame* TryToGetVideoFrame();
  void UpdateAudioBuffer(uint8_t* stream, int len, int audio_volume);

  stats_t GetStatistic() const;
  AVRational GetFrameRate() const;

 private:
  static int decode_interrupt_callback(void* user_data);
  stream_format_t GetStreamFormat() const;

  void StreamSeek(int64_t pos, int64_t rel, bool seek_by_bytes);
  frames::VideoFrame* GetVideoFrame();
  frames::VideoFrame* SelectVideoFrame() const;

  void ResetStats();
  void Close();

  bool IsVideoReady() const;
  bool IsAudioReady() const;

  DISALLOW_COPY_AND_ASSIGN(VideoState);

  /* open a given stream. Return 0 if OK */
  int StreamComponentOpen(int stream_index);
  void StreamComponentClose(int stream_index);
  void StreamTogglePause();

  AvSyncType GetMasterSyncType() const;
  clock64_t ComputeTargetDelay(clock64_t delay) const;
  clock64_t GetMasterPts() const;
  clock64_t GetMasterClock() const;
#if CONFIG_AVFILTER
  int ConfigureVideoFilters(AVFilterGraph* graph, const std::string& vfilters, AVFrame* frame);
  int ConfigureAudioFilters(const std::string& afilters, int force_output_format);
#endif

  /* return the wanted number of samples to get better sync if sync_type is
   * video
   * or external master clock */
  int SynchronizeAudio(int nb_samples);
  /**
   * Decode one audio frame and return its uncompressed size.
   *
   * The processed audio frame is decoded, converted if required, and
   * stored in is->audio_buf, with size in bytes given by the return
   * value.
   */
  int AudioDecodeFrame();
  int GetVideoFrame(AVFrame* frame);
  int QueuePicture(AVFrame* src_frame, clock64_t pts, clock64_t duration, int64_t pos);

  int ReadRoutine();
  int VideoThread();
  int AudioThread();

  const stream_id id_;
  const common::uri::GURL uri_;

  AppOptions opt_;
  ComplexOptions copt_;

  bool force_refresh_;
  int read_pause_return_;
  AVFormatContext* ic_;
  bool realtime_;

  VideoStream* vstream_;
  AudioStream* astream_;

  VideoDecoder* viddec_;
  AudioDecoder* auddec_;

  video_frame_queue_t* video_frame_queue_;
  audio_frame_queue_t* audio_frame_queue_;

  clock64_t audio_clock_;
  clock64_t audio_diff_cum_; /* used for AV difference average computation */
  double audio_diff_avg_coef_;
  double audio_diff_threshold_;
  int audio_diff_avg_count_;
  int audio_hw_buf_size_;
  uint8_t* audio_buf_;
  uint8_t* audio_buf1_;
  unsigned int audio_buf_size_; /* in bytes */
  unsigned int audio_buf1_size_;
  int audio_buf_index_; /* in bytes */
  int audio_write_buf_size_;
  AudioParams audio_src_;
#if CONFIG_AVFILTER
  AudioParams audio_filter_src_;
#endif
  AudioParams audio_tgt_;
  struct SwrContext* swr_ctx_;

  clock64_t frame_timer_;
  clock64_t frame_last_returned_time_;
  clock64_t frame_last_filter_delay_;
  clock64_t max_frame_duration_;  // maximum duration of a frame - above this, we
                                  // consider the jump a
                                  // timestamp discontinuity

  bool step_;

#if CONFIG_AVFILTER
  AVFilterContext* in_video_filter_;   // the first filter in the video chain
  AVFilterContext* out_video_filter_;  // the last filter in the video chain
  AVFilterContext* in_audio_filter_;   // the first filter in the audio chain
  AVFilterContext* out_audio_filter_;  // the last filter in the audio chain
  AVFilterGraph* agraph_;              // audio filter graph
#endif

  int last_video_stream_;
  int last_audio_stream_;

  std::shared_ptr<common::threads::Thread<int>> vdecoder_tid_;
  std::shared_ptr<common::threads::Thread<int>> adecoder_tid_;

  bool paused_;
  bool last_paused_;
  bool eof_;

  typedef std::unique_lock<std::mutex> lock_t;
  volatile bool abort_request_;

  stats_t stats_;
  VideoStateHandler* handler_;
  InputStream* input_st_;

  bool seek_req_;
  int64_t seek_pos_;
  int64_t seek_rel_;
  int seek_flags_;

  std::condition_variable read_thread_cond_;
  std::mutex read_thread_mutex_;
};

}  // namespace media
}  // namespace fastoplayer
