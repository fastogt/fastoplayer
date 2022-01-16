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

#include <player/isimple_player.h>

namespace fastoplayer {

class SimplePlayer : public ISimplePlayer {
 public:
  explicit SimplePlayer(const PlayerOptions& options);

  std::string GetCurrentUrlName() const override;

  void SetUrlLocation(media::stream_id sid,
                      const common::uri::Url& uri,
                      media::AppOptions opt,
                      media::ComplexOptions copt) override;

 private:
  common::uri::Url stream_url_;
};

}  // namespace fastoplayer
