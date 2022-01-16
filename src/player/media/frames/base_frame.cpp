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

#include <player/media/frames/base_frame.h>

namespace fastoplayer {
namespace media {
namespace frames {

BaseFrame::BaseFrame() : frame(av_frame_alloc()), pts(0), duration(0), pos(0) {}

BaseFrame::~BaseFrame() {
  ClearFrame();
  av_frame_free(&frame);
}

void BaseFrame::ClearFrame() {
  av_frame_unref(frame);
}

}  // namespace frames
}  // namespace media
}  // namespace fastoplayer
