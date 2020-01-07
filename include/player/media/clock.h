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

#include <player/media/types.h>  // for clock64_t

namespace fastoplayer {
namespace media {

class Clock {
 public:
  Clock();

  clock64_t GetPts() const;

  void SetClockAt(clock64_t pts, clock64_t time);
  void SetClock(clock64_t pts);
  clock64_t GetClock() const;

  clock64_t LastUpdated() const;

  void SetPaused(bool paused);

 private:
  bool paused_;
  clock64_t pts_;       /* clock base */
  clock64_t pts_drift_; /* clock base minus time at which we updated the clock */
  clock64_t last_updated_;
  double speed_;
};

}  // namespace media
}  // namespace fastoplayer
