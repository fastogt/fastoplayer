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

#include <player/draw/surface_saver.h>

#include <string>

#include <SDL2/SDL_image.h>

namespace fastoplayer {

namespace draw {

SurfaceSaver* MakeSurfaceFromPath(const std::string& img_full_path) {
  if (img_full_path.empty()) {
    return nullptr;
  }

  SDL_Surface* img_surface = IMG_Load(img_full_path.c_str());
  if (!img_surface) {
    return nullptr;
  }

  return new SurfaceSaver(img_surface);
}

SurfaceSaver::SurfaceSaver(SDL_Surface* surface) : surface_(surface), texture_(nullptr), renderer_(nullptr) {}

SurfaceSaver::~SurfaceSaver() {
  if (renderer_) {
    renderer_ = nullptr;
  }

  if (texture_) {
    SDL_DestroyTexture(texture_);
    texture_ = nullptr;
  }

  if (surface_) {
    SDL_FreeSurface(surface_);
    surface_ = nullptr;
  }
}

SDL_Texture* SurfaceSaver::GetTexture(SDL_Renderer* renderer) const {
  if (!renderer || !surface_) {
    return nullptr;
  }

  if (!texture_ || renderer_ != renderer) {
    if (texture_) {
      SDL_DestroyTexture(texture_);
      texture_ = nullptr;
    }

    texture_ = SDL_CreateTextureFromSurface(renderer, surface_);
    renderer_ = renderer;
  }

  return texture_;
}

int SurfaceSaver::GetWidthSurface() const {
  if (!surface_) {
    return 0;
  }

  return surface_->w;
}

int SurfaceSaver::GetHeightSurface() const {
  if (!surface_) {
    return 0;
  }

  return surface_->h;
}

}  // namespace draw

}  // namespace fastoplayer
