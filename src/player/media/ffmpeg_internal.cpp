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

#include <player/media/ffmpeg_internal.h>

#if CONFIG_CUVID
#include <player/media/hwaccels/ffmpeg_cuvid.h>
#endif
#if CONFIG_VIDEOTOOLBOX
#include <player/media/hwaccels/ffmpeg_videotoolbox.h>
#endif

namespace fastoplayer {
namespace media {

AVBufferRef* hw_device_ctx = nullptr;

const HWAccel hwaccels[] = {
#if CONFIG_VIDEOTOOLBOX
    {"videotoolbox", videotoolbox_init, videotoolbox_uninit, HWACCEL_VIDEOTOOLBOX, AV_HWDEVICE_TYPE_VIDEOTOOLBOX,
     AV_PIX_FMT_VIDEOTOOLBOX},
#endif
#if CONFIG_LIBMFX
    {"qsv", qsv_init, qsv_uninit, HWACCEL_QSV, AV_HWDEVICE_TYPE_QSV, AV_PIX_FMT_QSV},
#endif
#if CONFIG_CUVID
    {"cuvid", cuvid_init, cuvid_uninit, HWACCEL_CUVID, AV_HWDEVICE_TYPE_CUDA, AV_PIX_FMT_CUDA},
#endif
    HWAccel()};

size_t hwaccel_count() {
  return SIZEOFMASS(hwaccels) - 1;
}

const HWAccel* get_hwaccel(enum AVPixelFormat pix_fmt) {
  for (size_t i = 0; i < hwaccel_count(); i++) {
    if (hwaccels[i].pix_fmt == pix_fmt) {
      return &hwaccels[i];
    }
  }
  return nullptr;
}

}  // namespace media
}  // namespace fastoplayer
