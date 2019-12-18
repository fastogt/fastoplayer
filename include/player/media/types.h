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

#pragma once

#include <common/media/types.h>
#include <common/types.h>  // for time64_t
#include <common/uri/url.h>

#include <common/bounded_value.h>

#define DEFAULT_FRAME_PER_SEC 25

namespace fastoplayer {
namespace media {

typedef common::BoundedValue<int8_t, 0, 100> audio_volume_t;
typedef std::string stream_id;  // must be unique
extern const stream_id invalid_stream_id;

enum HWAccelID { HWACCEL_NONE = 0, HWACCEL_AUTO, HWACCEL_GENERIC, HWACCEL_VIDEOTOOLBOX, HWACCEL_QSV, HWACCEL_CUVID };
enum HWDeviceType {
  HWDEVICE_TYPE_NONE,
  HWDEVICE_TYPE_VDPAU,
  HWDEVICE_TYPE_CUDA,
  HWDEVICE_TYPE_VAAPI,
  HWDEVICE_TYPE_DXVA2,
  HWDEVICE_TYPE_QSV,
  HWDEVICE_TYPE_VIDEOTOOLBOX,
  HWDEVICE_TYPE_D3D11VA,
  HWDEVICE_TYPE_DRM,
  HWDEVICE_TYPE_OPENCL
};

typedef common::time64_t msec_t;
typedef common::time64_t clock64_t;
clock64_t invalid_clock();

common::media::bandwidth_t CalculateBandwidth(size_t total_downloaded_bytes, msec_t data_interval);

bool IsValidClock(clock64_t clock);
clock64_t GetRealClockTime();  // msec

msec_t ClockToMsec(clock64_t clock);
msec_t GetCurrentMsec();

typedef clock64_t pts_t;
pts_t invalid_pts();
bool IsValidPts(pts_t pts);

std::string make_url(const common::uri::Url& uri);

enum AvSyncType {
  AV_SYNC_AUDIO_MASTER, /* default choice */
  AV_SYNC_VIDEO_MASTER
};

int64_t get_valid_channel_layout(int64_t channel_layout, int channels);

std::string HWAccelIDToString(const HWAccelID& value, HWDeviceType dtype);
bool HWAccelIDFromString(const std::string& from, HWAccelID* out, HWDeviceType* dtype);
}  // namespace media
}  // namespace fastoplayer

namespace common {
std::string ConvertToString(fastoplayer::media::HWDeviceType value);
bool ConvertFromString(const std::string& from, fastoplayer::media::HWDeviceType* out);
}  // namespace common
