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

#include <player/gui/widgets/label.h>

namespace fastoplayer {
namespace gui {

class IconLabel : public Label {
 public:
  typedef Label base_class;
  enum { default_space = 1 };

  explicit IconLabel(Window* parent = nullptr);
  explicit IconLabel(const SDL_Color& back_ground_color, Window* parent = nullptr);
  ~IconLabel() override;

  void SetSpace(int space);
  int GetSpace() const;

  void SetIconSize(const common::draw::Size& icon_size);
  common::draw::Size GetIconSize() const;

  void SetIconTexture(SDL_Texture* icon_img);
  SDL_Texture* GetIconTexture() const;

  void Draw(SDL_Renderer* render) override;

 protected:
  void DrawImage(SDL_Renderer* render, SDL_Texture* texture, const SDL_Rect& rect);

 private:
  SDL_Texture* icon_img_;
  common::draw::Size icon_size_;
  int space_betwen_image_and_label_;
};

}  // namespace gui
}  // namespace fastoplayer
