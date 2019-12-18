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

#pragma once

#include <memory>
#include <string>

#include <SDL2/SDL_ttf.h>  // for TTF_Font

#include <common/file_system/path.h>
#include <common/uri/url.h>

#include <player/gui/events/events.h>  // for PostExecEvent, PreExe...
#include <player/gui/events/key_events.h>
#include <player/gui/events/mouse_events.h>
#include <player/gui/events/window_events.h>
#include <player/gui/lirc_events.h>
#include <player/gui/stream_events.h>
#include <player/media/app_options.h>  // for AppOptions, ComplexOp...
#include <player/media/types.h>
#include <player/player_options.h>
#include <player/stream_handler.h>

namespace common {
namespace threads {
template <typename RT>
class Thread;
}
}  // namespace common

namespace fastoplayer {

namespace draw {
class TextureSaver;
}
namespace media {
struct AudioParams;
}  // namespace media

namespace gui {
class Label;
}

class ISimplePlayer : public StreamHandler, public gui::events::EventListener {
 public:
  typedef common::Optional<common::file_system::ascii_file_string_path> file_string_path_t;
  enum {
    volume_height = 30,
    space_height = 10,
    space_width = 10,
    x_start = 10,
    y_start = 10,
    update_stats_timeout_msec = 1000,
    no_data_panic_sec = 60
  };
  static const SDL_Color text_color;
  static const AVRational min_fps;

  static const SDL_Color volume_color;
  static const SDL_Color stream_statistic_color;

  enum States { INIT_STATE, PLAYING_STATE, FAILED_STATE };

  void SetFullScreen(bool full_screen);
  void SetMute(bool mute);
  void UpdateVolume(int8_t step);
  void Quit();

  ~ISimplePlayer() override;

  States GetCurrentState() const;

  virtual std::string GetCurrentUrlName() const = 0;

  PlayerOptions GetOptions() const;

  virtual void SetUrlLocation(media::stream_id sid,
                              const common::uri::Url& uri,
                              media::AppOptions opt,
                              media::ComplexOptions copt);

 protected:
  ISimplePlayer(const PlayerOptions& options, const file_string_path_t& absolute_font_path);

  void HandleEvent(event_t* event) override;
  void HandleExceptionEvent(event_t* event, common::Error err) override;

  common::Error HandleRequestAudio(media::VideoState* stream,
                                   int64_t wanted_channel_layout,
                                   int wanted_nb_channels,
                                   int wanted_sample_rate,
                                   media::AudioParams* audio_hw_params,
                                   int* audio_buff_size) override;
  void HanleAudioMix(uint8_t* audio_stream_ptr, const uint8_t* src, uint32_t len, int volume) override;

  // should executed in gui thread
  common::Error HandleRequestVideo(media::VideoState* stream,
                                   int width,
                                   int height,
                                   int av_pixel_format,
                                   AVRational aspect_ratio) override;

  virtual void HandlePreExecEvent(gui::events::PreExecEvent* event);
  virtual void HandlePostExecEvent(gui::events::PostExecEvent* event);

  virtual void HandleTimerEvent(gui::events::TimerEvent* event);

  virtual void HandleRequestVideoEvent(gui::events::RequestVideoEvent* event);
  virtual void HandleQuitStreamEvent(gui::events::QuitStreamEvent* event);

  virtual void HandleKeyPressEvent(gui::events::KeyPressEvent* event);

  virtual void HandleLircPressEvent(gui::events::LircPressEvent* event);

  virtual void HandleWindowResizeEvent(gui::events::WindowResizeEvent* event);
  virtual void HandleWindowExposeEvent(gui::events::WindowExposeEvent* event);
  virtual void HandleWindowCloseEvent(gui::events::WindowCloseEvent* event);

  virtual void HandleMousePressEvent(gui::events::MousePressEvent* event);
  virtual void HandleMouseMoveEvent(gui::events::MouseMoveEvent* event);

  virtual void HandleQuitEvent(gui::events::QuitEvent* event);

  virtual void InitWindow(const std::string& title, States status);
  virtual void SetStatus(States new_state);

  virtual void DrawDisplay();
  virtual void DrawPlayingStatus();
  virtual void DrawFailedStatus();
  virtual void DrawInitStatus();

  virtual void DrawInfo();  // statistic + volume

  virtual void DrawStatistic();
  virtual void DrawVolume();

  bool IsMouseVisible() const;

  virtual media::VideoState* CreateStream(media::stream_id sid,
                                          const common::uri::Url& uri,
                                          media::AppOptions opt,
                                          media::ComplexOptions copt);
  void SetStream(media::VideoState* stream);  // if stream == nullptr => SwitchToChannelErrorMode

  SDL_Rect GetDrawRect() const;  // GetDisplayRect + with margins
  SDL_Rect GetDisplayRect() const;

  SDL_Renderer* GetRenderer() const;
  TTF_Font* GetFont() const;

  virtual void OnWindowCreated(SDL_Window* window, SDL_Renderer* render);

 private:
  void SwitchToChannelErrorMode(common::Error err);

  void FreeStreamSafe(bool fast_cleanup);

  void UpdateDisplayInterval(AVRational fps);

  /* prepare a new audio buffer */
  static void sdl_audio_callback(void* user_data, uint8_t* stream, int len);

  void CalculateDispalySize();

  // channel events
  void ToggleMute();
  void PauseStream();

  // player modes
  void ToggleShowStatistic();

  SDL_Rect GetStatisticRect() const;
  SDL_Rect GetVolumeRect() const;

  SDL_Renderer* renderer_;
  TTF_Font* font_;

  PlayerOptions options_;

  media::AudioParams* audio_params_;
  int audio_buff_size_;
  SDL_AudioDeviceID audio_device_;

  SDL_Window* window_;

  media::msec_t cursor_last_shown_;

  gui::Label* volume_label_;
  media::msec_t volume_last_shown_;

  media::msec_t last_mouse_left_click_;

  std::shared_ptr<common::threads::Thread<int>> exec_tid_;
  media::VideoState* stream_;

  common::draw::Size window_size_;
  const int xleft_;
  const int ytop_;

  States current_state_;

  bool muted_;
  gui::Label* statistic_label_;

  draw::TextureSaver* render_texture_;

  uint32_t update_video_timer_interval_msec_;

  media::clock64_t last_pts_checkpoint_;
  size_t video_frames_handled_;
  const file_string_path_t absolute_font_path_;
};

}  // namespace fastoplayer
