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

#include <player/draw/font.h>
#include <player/gui/widgets/window.h>

namespace fastoplayer {
namespace gui {

class FontWindow : public Window {
 public:
  typedef Window base_class;
  enum DrawType { WRAPPED_TEXT, CENTER_TEXT };

  explicit FontWindow(Window* parent = nullptr);
  explicit FontWindow(const SDL_Color& back_ground_color, Window* parent = nullptr);
  virtual ~FontWindow();

  void SetDrawType(DrawType dt);
  DrawType GetDrawType() const;

  void SetTextColor(const SDL_Color& color);
  SDL_Color GetTextColor() const;

  void SetFont(TTF_Font* font);
  TTF_Font* GetFont() const;

 protected:
  void DrawText(SDL_Renderer* render,
                const std::string& text,
                const SDL_Rect& rect,
                DrawType dt,
                SDL_Rect* text_rect = nullptr);

 private:
  DrawType draw_type_;
  SDL_Color text_color_;
  TTF_Font* font_;
};

}  // namespace gui
}  // namespace fastoplayer
