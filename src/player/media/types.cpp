/*  Copyright (C) 2014-2019 FastoGT. All right reserved.

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

#include <player/media/types.h>

#include <algorithm>

extern "C" {
#include <libavutil/avutil.h>          // for AV_NOPTS_VALUE
#include <libavutil/channel_layout.h>  // for av_get_channel_layout_nb_channels
}

#include <common/time.h>  // for current_mstime

#include <player/media/ffmpeg_internal.h>

namespace fastoplayer {
namespace media {

const stream_id invalid_stream_id = stream_id();

common::media::bandwidth_t CalculateBandwidth(size_t total_downloaded_bytes, msec_t data_interval) {
  if (data_interval == 0) {
    return 0;
  }

  common::media::bandwidth_t bytes_per_msec = total_downloaded_bytes / data_interval;
  return bytes_per_msec * 1000;
}

clock64_t invalid_clock() {
  return -1;
}

bool IsValidClock(clock64_t clock) {
  return clock != invalid_clock();
}

clock64_t GetRealClockTime() {
  return GetCurrentMsec();
}

msec_t ClockToMsec(clock64_t clock) {
  return clock;
}

msec_t GetCurrentMsec() {
  return common::time::current_utc_mstime();
}

int64_t get_valid_channel_layout(int64_t channel_layout, int channels) {
  if (channel_layout && av_get_channel_layout_nb_channels(channel_layout) == channels) {
    return channel_layout;
  }
  return 0;
}

std::string make_url(const common::uri::Url& uri) {
  if (uri.GetScheme() == common::uri::Url::file) {
    common::uri::Upath upath = uri.GetPath();
    return upath.GetPath();
  }

  return uri.GetUrl();
}

pts_t invalid_pts() {
  return AV_NOPTS_VALUE;
}

bool IsValidPts(pts_t pts) {
  return pts != invalid_pts();
}

std::string HWAccelIDToString(const HWAccelID& value, HWDeviceType dtype) {
  if (value == HWACCEL_AUTO) {
    return "auto";
  } else if (value == HWACCEL_NONE) {
    return "none";
  }

  for (size_t i = 0; i < hwaccel_count(); i++) {
    if (value == hwaccels[i].id) {
      return hwaccels[i].name;
    }
  }

  return common::ConvertToString(dtype);
}

bool HWAccelIDFromString(const std::string& from, HWAccelID* out, HWDeviceType* dtype) {
  if (from.empty() || !out || !dtype) {
    return false;
  }

  std::string from_copy = from;
  std::transform(from_copy.begin(), from_copy.end(), from_copy.begin(), ::tolower);
  if (from_copy == "auto") {
    *out = HWACCEL_AUTO;
    *dtype = HWDEVICE_TYPE_NONE;
    return true;
  } else if (from_copy == "none") {
    *out = HWACCEL_NONE;
    *dtype = HWDEVICE_TYPE_NONE;
    return true;
  }

  const char* from_copy_ptr = from.c_str();
  for (size_t i = 0; i < hwaccel_count(); i++) {
    if (strcmp(hwaccels[i].name, from_copy_ptr) == 0) {
      *out = hwaccels[i].id;
      *dtype = HWDEVICE_TYPE_NONE;
      return true;
    }
  }

  HWDeviceType ctype;
  if (common::ConvertFromString(from, &ctype) && ctype != HWDEVICE_TYPE_NONE) {
    *out = HWACCEL_GENERIC;
    *dtype = ctype;
    return true;
  }
  return false;
}

}  // namespace media
}  // namespace fastoplayer

namespace common {

std::string ConvertToString(fastoplayer::media::HWDeviceType value) {
  const char* ptr = av_hwdevice_get_type_name(static_cast<AVHWDeviceType>(value));
  if (ptr) {
    return ptr;
  }
  return "none";
}

bool ConvertFromString(const std::string& from, fastoplayer::media::HWDeviceType* out) {
  if (from.empty() || !out) {
    return false;
  }

  const char* from_copy_ptr = from.c_str();
  AVHWDeviceType type = av_hwdevice_find_type_by_name(from_copy_ptr);
  *out = static_cast<fastoplayer::media::HWDeviceType>(type);
  return true;
}

}  // namespace common
