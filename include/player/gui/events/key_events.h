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

#include <string>

#include <SDL2/SDL_keyboard.h>

#include <player/gui/events_base.h>

namespace fastoplayer {
namespace gui {
namespace events {

struct KeyPressInfo {
  KeyPressInfo(bool pressed, SDL_Keysym ks);

  bool is_pressed;
  SDL_Keysym ks;
};

struct KeyReleaseInfo {
  KeyReleaseInfo(bool pressed, SDL_Keysym ks);

  bool is_pressed;
  SDL_Keysym ks;
};

struct TextInputInfo {
  explicit TextInputInfo(const std::string& text);

  std::string text;
};

struct TextEditInfo {
  TextEditInfo(const std::string& text, Sint32 start, Sint32 length);

  std::string text;
  Sint32 start;  /**< The start cursor of selected editing text */
  Sint32 length; /**< The length of selected editing text */
};

typedef EventBase<KEY_PRESS_EVENT, KeyPressInfo> KeyPressEvent;
typedef EventBase<KEY_RELEASE_EVENT, KeyReleaseInfo> KeyReleaseEvent;
typedef EventBase<TEXT_INPUT_EVENT, TextInputInfo> TextInputEvent;
typedef EventBase<TEXT_EDIT_EVENT, TextEditInfo> TextEditEvent;

}  // namespace events
}  // namespace gui
}  // namespace fastoplayer
