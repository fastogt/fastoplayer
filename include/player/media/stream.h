/*  Copyright (C) 2014-2020 FastoGT. All right reserved.

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

#include <player/media/ffmpeg_config.h>

extern "C" {
#include <libavcodec/avcodec.h>    // for AVPacket
#include <libavformat/avformat.h>  // for AVStream
#include <libavutil/rational.h>    // for AVRational
}

#include <common/macros.h>                      // for DISALLOW_COPY_AND_ASSIGN
#include <common/media/bandwidth_estimation.h>  // for DesireBytesPerSec
#include <common/types.h>                       // for time64_t

#include <player/media/types.h>  // for clock64_t

namespace fastoplayer {
namespace media {

class Clock;
class PacketQueue;

class Stream {
 public:
  enum { minimum_frames = 25 };
  bool HasEnoughPackets() const;
  virtual bool Open(int index, AVStream* av_stream_st);
  bool IsOpened() const;
  virtual void Close();
  virtual ~Stream();

  int Index() const;
  AVRational GetTimeBase() const;
  AVCodecParameters* GetCodecpar() const;
  double q2d() const;

  clock64_t GetPts() const;

  // clock interface
  clock64_t GetClock() const;

  void SetClockAt(clock64_t pts, clock64_t time);
  void SetClock(clock64_t pts);
  void SetPaused(bool pause);

  clock64_t LastUpdatedClock() const;

  void SyncSerialClock();

  PacketQueue* GetQueue() const;
  common::media::bandwidth_t Bandwidth() const;
  common::media::DesireBytesPerSec DesireBandwith() const;
  size_t TotalDownloadedBytes() const;
  void RegisterPacket(const AVPacket* packet);

 protected:
  Stream();
  void SetDesireBandwith(const common::media::DesireBytesPerSec& band);

  AVStream* stream_st_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Stream);

  PacketQueue* packet_queue_;
  Clock* clock_;
  int stream_index_;

  common::media::DesireBytesPerSec bandwidth_;
  common::time64_t start_ts_;
  size_t total_downloaded_bytes_;
};

class VideoStream : public Stream {
 public:
  VideoStream();

  virtual bool Open(int index, AVStream* av_stream_st, AVRational frame_rate);

  AVRational GetFrameRate() const;
  double GetRotation() const;
  bool HaveDispositionPicture() const;
  AVRational GetAspectRatio() const;
  AVRational StableAspectRatio(AVFrame* frame) const;

 private:
  using Stream::Open;
  AVRational frame_rate_;
};

class AudioStream : public Stream {
 public:
  AudioStream();

  virtual bool Open(int index, AVStream* av_stream_st) override;
};

}  // namespace media
}  // namespace fastoplayer
