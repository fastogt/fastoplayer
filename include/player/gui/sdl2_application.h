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

#include <SDL2/SDL_events.h>  // for SDL_MouseButtonEvent
#include <SDL2/SDL_stdinc.h>  // for Uint32

#include <common/application/application.h>   // for IApplicationImpl
#include <common/event.h>                     // for IListener (ptr only)
#include <common/threads/event_dispatcher.h>  // for EventDispatcher

#include <player/gui/events_base.h>  // for Event, EventsType
#include <player/media/types.h>

namespace fastoplayer {
namespace gui {
namespace application {

class Sdl2Application : public common::application::IApplication {
 public:
  typedef Uint32 update_display_timeout_t;
  enum { event_timeout_wait_msec = 1000 / DEFAULT_FRAME_PER_SEC };
  Sdl2Application(int argc, char** argv);
  ~Sdl2Application();

  void Subscribe(common::IListener* listener, common::events_size_t id) override;
  void UnSubscribe(common::IListener* listener, common::events_size_t id) override;
  void UnSubscribe(common::IListener* listener) override;

  void SendEvent(common::IEvent* event) override;
  void PostEvent(common::IEvent* event) override;

  void ShowCursor() override;
  void HideCursor() override;
  bool IsCursorVisible() const override;

  common::application::timer_id_t AddTimer(uint32_t interval,
                                           common::application::timer_callback_t cb,
                                           void* user_data) override;
  bool RemoveTimer(common::application::timer_id_t id) override;

  update_display_timeout_t GetDisplayUpdateTimeout() const;
  void SetDisplayUpdateTimeout(update_display_timeout_t msec);

 protected:
  virtual void HandleEvent(gui::events::Event* event);

  virtual void HandleKeyDownEvent(SDL_KeyboardEvent* event);
  virtual void HandleKeyUpEvent(SDL_KeyboardEvent* event);
  virtual void HandleTextInputEvent(SDL_TextInputEvent* event);
  virtual void HandleTextEditEvent(SDL_TextEditingEvent* event);

  virtual void HandleWindowEvent(SDL_WindowEvent* event);

  virtual void HandleMouseMoveEvent(SDL_MouseMotionEvent* event);

  virtual void HandleMousePressEvent(SDL_MouseButtonEvent* event);
  virtual void HandleMouseReleaseEvent(SDL_MouseButtonEvent* event);

  virtual void HandleQuitEvent(SDL_QuitEvent* event);

  int PreExecImpl() override;   // EXIT_FAILURE, EXIT_SUCCESS
  int ExecImpl() override;      // EXIT_FAILURE, EXIT_SUCCESS
  int PostExecImpl() override;  // EXIT_FAILURE, EXIT_SUCCESS
  void ExitImpl(int result) override;

 private:
  void ProcessEvent(SDL_Event* event);

  common::threads::EventDispatcher<EventsType> dispatcher_;
  update_display_timeout_t update_display_timeout_msec_;
  bool cursor_visible_;
};

}  // namespace application
}  // namespace gui
}  // namespace fastoplayer
