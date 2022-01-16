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

#include <player/gui/sdl2_application.h>

namespace fastoplayer {

class FFmpegApplication : public gui::application::Sdl2Application {
 public:
  typedef gui::application::Sdl2Application base_class;
  FFmpegApplication(int argc, char** argv);

  ~FFmpegApplication();

 private:
  int PreExecImpl() override;

  int PostExecImpl() override;
};

int prepare_to_start(const std::string& app_directory_absolute_path);

}  // namespace fastoplayer
