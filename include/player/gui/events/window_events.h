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

#include <common/draw/size.h>

#include <player/draw/types.h>
#include <player/gui/events_base.h>

namespace fastoplayer {
namespace gui {
namespace events {

struct WindowResizeInfo {
  explicit WindowResizeInfo(const common::draw::Size& size);

  common::draw::Size size;
};

struct WindowExposeInfo {};
struct WindowCloseInfo {};

typedef EventBase<WINDOW_RESIZE_EVENT, WindowResizeInfo> WindowResizeEvent;
typedef EventBase<WINDOW_EXPOSE_EVENT, WindowExposeInfo> WindowExposeEvent;
typedef EventBase<WINDOW_CLOSE_EVENT, WindowCloseInfo> WindowCloseEvent;

}  // namespace events
}  // namespace gui
}  // namespace fastoplayer
