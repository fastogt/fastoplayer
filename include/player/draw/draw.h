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

#include <string>

#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_render.h>  // for SDL_Renderer, SDL_Texture

#include <common/draw/size.h>
#include <common/error.h>

#include <player/draw/types.h>

namespace fastoplayer {
namespace draw {

common::Error CreateMainWindow(const common::draw::Size& size,
                               bool is_full_screen,
                               const std::string& title,
                               SDL_Renderer** renderer,
                               SDL_Window** window) WARN_UNUSED_RESULT;

common::Error CreateTexture(SDL_Renderer* renderer,
                            Uint32 new_format,
                            int new_width,
                            int new_height,
                            SDL_BlendMode blendmode,
                            bool init_texture,
                            SDL_Texture** texture_out) WARN_UNUSED_RESULT;

common::Error SetRenderDrawColor(SDL_Renderer* render, const SDL_Color& rgba) WARN_UNUSED_RESULT;
common::Error FillRectColor(SDL_Renderer* render, const SDL_Rect& rect, const SDL_Color& rgba) WARN_UNUSED_RESULT;
common::Error DrawBorder(SDL_Renderer* render, const SDL_Rect& rect, const SDL_Color& rgba) WARN_UNUSED_RESULT;
common::Error FlushRender(SDL_Renderer* render, const SDL_Color& rgba) WARN_UNUSED_RESULT;

}  // namespace draw
}  // namespace fastoplayer
