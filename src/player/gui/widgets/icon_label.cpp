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

#include <player/gui/widgets/icon_label.h>

namespace fastoplayer {

namespace gui {

IconLabel::IconLabel(Window* parent)
    : base_class(parent), icon_img_(nullptr), icon_size_(), space_betwen_image_and_label_(default_space) {}

IconLabel::IconLabel(const SDL_Color& back_ground_color, Window* parent)
    : base_class(back_ground_color, parent),
      icon_img_(nullptr),
      icon_size_(),
      space_betwen_image_and_label_(default_space) {}

IconLabel::~IconLabel() {}

void IconLabel::SetSpace(int space) {
  space_betwen_image_and_label_ = space;
}

int IconLabel::GetSpace() const {
  return space_betwen_image_and_label_;
}

void IconLabel::Draw(SDL_Renderer* render) {
  if (!IsCanDraw()) {
    base_class::Draw(render);
    return;
  }

  if (!icon_img_) {
    base_class::Draw(render);
    return;
  }

  FontWindow::Draw(render);
  SDL_Rect area_rect = GetRect();
  SDL_Rect icon_rect = {area_rect.x, area_rect.y, icon_size_.height, icon_size_.width};

  int shift = icon_size_.width + space_betwen_image_and_label_;
  DrawImage(render, icon_img_, icon_rect);
  SDL_Rect text_rect = {area_rect.x + shift, area_rect.y, area_rect.w - shift, area_rect.h};
  const std::string text = GetText();
  base_class::DrawText(render, text, text_rect, GetDrawType());
}

void IconLabel::DrawImage(SDL_Renderer* render, SDL_Texture* texture, const SDL_Rect& rect) {
  draw::DrawImage(render, texture, rect);
}

void IconLabel::SetIconSize(const common::draw::Size& icon_size) {
  icon_size_ = icon_size;
}

common::draw::Size IconLabel::GetIconSize() const {
  return icon_size_;
}

void IconLabel::SetIconTexture(SDL_Texture* icon_img) {
  icon_img_ = icon_img;
}

SDL_Texture* IconLabel::GetIconTexture() const {
  return icon_img_;
}

}  // namespace gui

}  // namespace fastoplayer
