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

#include <player/gui/events/mouse_events.h>

namespace fastoplayer {

namespace gui {
namespace events {

MouseStateChangeInfo::MouseStateChangeInfo(const SDL_Point& mouse_point, bool cursor_visible)
    : position(mouse_point), cursor_visible(cursor_visible) {}

SDL_Point MouseStateChangeInfo::GetMousePoint() const {
  return position;
}

bool MouseStateChangeInfo::IsCursorVisible() const {
  return cursor_visible;
}

MouseMoveInfo::MouseMoveInfo(const SDL_MouseMotionEvent& event) : mevent(event) {}

SDL_Point MouseMoveInfo::GetMousePoint() const {
  return {mevent.x, mevent.y};
}

MousePressInfo::MousePressInfo(const SDL_MouseButtonEvent& event) : mevent(event) {}

SDL_Point MousePressInfo::GetMousePoint() const {
  return {mevent.x, mevent.y};
}

MouseReleaseInfo::MouseReleaseInfo(const SDL_MouseButtonEvent& event) : mevent(event) {}

SDL_Point MouseReleaseInfo::GetMousePoint() const {
  return {mevent.x, mevent.y};
}

}  // namespace events
}  // namespace gui

}  // namespace fastoplayer
