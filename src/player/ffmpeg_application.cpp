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

#include <player/ffmpeg_application.h>

#include <signal.h>

#include <iostream>

extern "C" {
#include <libavformat/avformat.h>  // for av_register_all, avforma...
}

#include <common/file_system/file_system.h>
#include <common/file_system/string_path_utils.h>
#include <common/string_util.h>

#include <player/gui/events/events.h>

namespace {

void __attribute__((noreturn)) sigterm_handler(int sig) {
  UNUSED(sig);
  exit(EXIT_FAILURE);
}

int fasto_log_to_ffmpeg(common::logging::LOG_LEVEL level) {
  if (level <= common::logging::LOG_LEVEL_CRIT) {
    return AV_LOG_FATAL;
  } else if (level <= common::logging::LOG_LEVEL_ERR) {
    return AV_LOG_ERROR;
  } else if (level <= common::logging::LOG_LEVEL_WARNING) {
    return AV_LOG_WARNING;
  } else if (level <= common::logging::LOG_LEVEL_INFO) {
    return AV_LOG_INFO;
  } else {
    return common::logging::LOG_LEVEL_DEBUG;
  }
}

common::logging::LOG_LEVEL ffmpeg_log_to_fasto(int level) {
  if (level <= AV_LOG_FATAL) {
    return common::logging::LOG_LEVEL_CRIT;
  } else if (level <= AV_LOG_ERROR) {
    return common::logging::LOG_LEVEL_ERR;
  } else if (level <= AV_LOG_WARNING) {
    return common::logging::LOG_LEVEL_WARNING;
  } else if (level <= AV_LOG_INFO) {
    return common::logging::LOG_LEVEL_INFO;
  } else {
    return common::logging::LOG_LEVEL_DEBUG;
  }
}

void avlog_cb(void*, int level, const char* sz_fmt, va_list varg) {
  common::logging::LOG_LEVEL lg = ffmpeg_log_to_fasto(level);
  common::logging::LOG_LEVEL clg = common::logging::CURRENT_LOG_LEVEL();
  if (lg > clg) {
    return;
  }

  char* ret = nullptr;
  int res = common::vasprintf(&ret, sz_fmt, varg);
  if (res == ERROR_RESULT_VALUE || !ret) {
    return;
  }

  static common::logging::LogMessage info_msg(common::logging::LOG_LEVEL_INFO, false);
  info_msg.Stream() << ret;
  free(ret);
}

}  // namespace

namespace fastoplayer {

FFmpegApplication::FFmpegApplication(int argc, char** argv) : base_class(argc, argv) {
  avformat_network_init();
  signal(SIGINT, sigterm_handler);  /* Interrupt (ANSI).    */
  signal(SIGTERM, sigterm_handler); /* Termination (ANSI).  */

  av_log_set_callback(avlog_cb);
  int ffmpeg_log_level = fasto_log_to_ffmpeg(common::logging::CURRENT_LOG_LEVEL());
  av_log_set_level(ffmpeg_log_level);
}

FFmpegApplication::~FFmpegApplication() {
  avformat_network_deinit();
}

int FFmpegApplication::PreExecImpl() {
  int pre_exec = base_class::PreExecImpl();
  gui::events::PreExecInfo inf(pre_exec);
  gui::events::PreExecEvent* pre_event = new gui::events::PreExecEvent(this, inf);
  base_class::SendEvent(pre_event);
  return pre_exec;
}

int FFmpegApplication::PostExecImpl() {
  gui::events::PostExecInfo inf(EXIT_SUCCESS);
  gui::events::PostExecEvent* post_event = new gui::events::PostExecEvent(this, inf);
  base_class::SendEvent(post_event);
  return base_class::PostExecImpl();
}

int prepare_to_start(const std::string& app_directory_absolute_path) {
  if (!common::file_system::is_directory_exist(app_directory_absolute_path)) {
    common::ErrnoError err = common::file_system::create_directory(app_directory_absolute_path, true);
    if (err) {
      std::cout << "Can't create app directory error:(" << err->GetDescription()
                << "), path: " << app_directory_absolute_path << std::endl;
      return EXIT_FAILURE;
    }
  }

  common::ErrnoError err = common::file_system::node_access(app_directory_absolute_path);
  if (err) {
    std::cout << "Can't have permissions to app directory path: " << app_directory_absolute_path << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

}  // namespace fastoplayer
