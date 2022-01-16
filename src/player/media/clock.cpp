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

#include <player/media/clock.h>

namespace fastoplayer {
namespace media {

Clock::Clock() : paused_(false), speed_(1.0) {
  SetClock(invalid_clock());
}

void Clock::SetClockAt(clock64_t pts, clock64_t time) {
  pts_ = pts;
  last_updated_ = time;
  pts_drift_ = pts - time;
}

void Clock::SetClock(clock64_t pts) {
  clock64_t time = GetRealClockTime();
  SetClockAt(pts, time);
}

clock64_t Clock::GetPts() const {
  return pts_;
}

clock64_t Clock::GetClock() const {
  if (paused_) {
    return pts_;
  }

  clock64_t time = GetRealClockTime();
  return pts_drift_ + time - (time - last_updated_) * (1.0 - speed_);
}

clock64_t Clock::LastUpdated() const {
  return last_updated_;
}

void Clock::SetPaused(bool paused) {
  paused_ = paused;
}

}  // namespace media
}  // namespace fastoplayer
