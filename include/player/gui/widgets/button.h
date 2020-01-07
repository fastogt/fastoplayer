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

#pragma once

#include <player/gui/widgets/icon_label.h>

namespace fastoplayer {
namespace gui {

class Button : public IconLabel {
 public:
  typedef IconLabel base_class;

  explicit Button(Window* parent = nullptr);
  explicit Button(const SDL_Color& back_ground_color, Window* parent = nullptr);
  ~Button() override;

  bool IsPressed() const;

  void Draw(SDL_Renderer* render) override;

 protected:
  void OnFocusChanged(bool focus) override;
  void OnMouseClicked(Uint8 button, const SDL_Point& position) override;
  void OnMouseReleased(Uint8 button, const SDL_Point& position) override;

 private:
  bool pressed_;
};

}  // namespace gui
}  // namespace fastoplayer
