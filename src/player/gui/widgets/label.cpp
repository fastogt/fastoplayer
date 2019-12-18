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

#include <player/gui/widgets/label.h>

namespace fastoplayer {

namespace gui {

Label::Label(Window* parent) : base_class(parent), text_(), text_changed_cb_() {}

Label::Label(const SDL_Color& back_ground_color, Window* parent)
    : base_class(back_ground_color, parent), text_(), text_changed_cb_() {}

Label::~Label() {}

void Label::SetTextChangedCallback(text_changed_callback_t cb) {
  text_changed_cb_ = cb;
}

void Label::SetText(const std::string& text) {
  text_ = text;
  OnTextChanged(text);
}

std::string Label::GetText() const {
  return text_;
}

void Label::ClearText() {
  SetText(std::string());
}

void Label::Draw(SDL_Renderer* render) {
  if (!IsCanDraw()) {
    base_class::Draw(render);
    return;
  }

  DrawLabel(render, nullptr);
}

void Label::DrawLabel(SDL_Renderer* render, SDL_Rect* text_rect) {
  base_class::Draw(render);
  base_class::DrawText(render, text_, GetRect(), GetDrawType(), text_rect);
}

void Label::OnTextChanged(const std::string& text) {
  if (text_changed_cb_) {
    text_changed_cb_(text);
  }
}

}  // namespace gui

}  // namespace fastoplayer
