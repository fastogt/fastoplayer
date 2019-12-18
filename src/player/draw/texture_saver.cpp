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

#include <player/draw/texture_saver.h>

#include <player/draw/draw.h>

namespace fastoplayer {

namespace draw {

TextureSaver::TextureSaver() : texture_(nullptr), renderer_(nullptr) {}

int TextureSaver::GetWidth() const {
  Uint32 format;
  int access, w, h;
  if (SDL_QueryTexture(texture_, &format, &access, &w, &h) < 0) {
    return 0;
  }

  return w;
}

int TextureSaver::GetHeight() const {
  Uint32 format;
  int access, w, h;
  if (SDL_QueryTexture(texture_, &format, &access, &w, &h) < 0) {
    return 0;
  }

  return h;
}

SDL_Texture* TextureSaver::GetTexture(SDL_Renderer* renderer, int width, int height, Uint32 format) const {
  if (!renderer) {
    return nullptr;
  }

  Uint32 cur_format;
  int access, w, h;
  if (renderer_ != renderer || SDL_QueryTexture(texture_, &cur_format, &access, &w, &h) < 0 || width != w ||
      height != h || format != cur_format) {
    SDL_DestroyTexture(texture_);
    texture_ = nullptr;
    renderer_ = nullptr;

    SDL_Texture* ltexture = nullptr;
    common::Error err = CreateTexture(renderer, format, width, height, SDL_BLENDMODE_NONE, false, &ltexture);
    if (err) {
      DNOTREACHED() << err->GetDescription();
      return nullptr;
    }
    texture_ = ltexture;
    renderer_ = renderer;
  }

  return texture_;
}

TextureSaver::~TextureSaver() {
  if (texture_) {
    SDL_DestroyTexture(texture_);
    texture_ = nullptr;
  }
  renderer_ = nullptr;
}

}  // namespace draw

}  // namespace fastoplayer
