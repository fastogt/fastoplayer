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

#include "simple_player.h"

#include <string>

#include <common/file_system/string_path_utils.h>

namespace fastoplayer {

namespace {
ISimplePlayer::file_string_path_t MakeFontPath() {
  static const common::file_system::ascii_directory_string_path font_dir(
      common::file_system::absolute_path_from_relative(RELATIVE_FONT_DIR));
  return font_dir.MakeFileStringPath("FreeSans.ttf");
}
}  // namespace

SimplePlayer::SimplePlayer(const PlayerOptions& options) : ISimplePlayer(options, MakeFontPath()), stream_url_() {}

std::string SimplePlayer::GetCurrentUrlName() const {
  return stream_url_.GetUrl();
}

void SimplePlayer::SetUrlLocation(media::stream_id sid,
                                  const common::uri::Url& uri,
                                  media::AppOptions opt,
                                  media::ComplexOptions copt) {
  stream_url_ = uri;
  ISimplePlayer::SetUrlLocation(sid, uri, opt, copt);
}

}  // namespace fastoplayer
