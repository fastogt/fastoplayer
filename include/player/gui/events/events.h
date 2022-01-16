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

#include <common/types.h>  // for time64_t

#include <player/gui/events_base.h>

namespace fastoplayer {
namespace gui {
namespace events {

struct TimeInfo {
  TimeInfo();

  common::time64_t time_millisecond;
};

struct QuitInfo {};

struct PreExecInfo {
  explicit PreExecInfo(int code);
  int code;
};

struct PostExecInfo {
  explicit PostExecInfo(int code);
  int code;
};

typedef EventBase<PRE_EXEC_EVENT, PreExecInfo> PreExecEvent;
typedef EventBase<POST_EXEC_EVENT, PostExecInfo> PostExecEvent;

typedef EventBase<TIMER_EVENT, TimeInfo> TimerEvent;

typedef EventBase<QUIT_EVENT, QuitInfo> QuitEvent;

}  // namespace events
}  // namespace gui
}  // namespace fastoplayer
