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

#include <functional>

#include <SDL2/SDL_render.h>

#include <player/draw/types.h>
#include <player/gui/events/mouse_events.h>
#include <player/gui/events/window_events.h>

namespace fastoplayer {

namespace gui {

class Window : public gui::events::EventListener {
 public:
  typedef std::function<void(Uint8 button, const SDL_Point& position)> mouse_clicked_callback_t;
  typedef std::function<void(Uint8 button, const SDL_Point& position)> mouse_released_callback_t;
  typedef std::function<void(bool focus)> focus_changed_callback_t;
  typedef std::function<void(bool enable)> enable_changed_callback_t;

  explicit Window(Window* parent = nullptr);
  explicit Window(const SDL_Color& back_ground_color, Window* parent = nullptr);
  ~Window() override;

  void SetMouseClickedCallback(mouse_clicked_callback_t cb);
  void SetMouseReeleasedCallback(mouse_released_callback_t cb);
  void SetFocusChangedCallback(focus_changed_callback_t cb);
  void SetEnableChangedCallback(enable_changed_callback_t cb);

  void SetRect(const SDL_Rect& rect);
  SDL_Rect GetRect() const;

  SDL_Color GetBackGroundColor() const;
  void SetBackGroundColor(const SDL_Color& color);

  SDL_Color GetBorderColor() const;
  void SetBorderColor(const SDL_Color& color);

  common::draw::Size GetMinimalSize() const;
  void SetMinimalSize(const common::draw::Size& ms);

  bool IsTransparent() const;
  void SetTransparent(bool t);

  bool IsBordered() const;
  void SetBordered(bool b);

  void SetVisible(bool v);
  bool IsVisible() const;

  void SetEnabled(bool en);
  bool IsEnabled() const;

  bool IsFocused() const;
  void SetFocus(bool focus);

  void Show();
  void Hide();
  void ToggleVisible();

  virtual void Draw(SDL_Renderer* render);

  bool IsCanDraw() const;
  bool IsSizeEnough() const;

 protected:
  void HandleEvent(event_t* event) override;
  void HandleExceptionEvent(event_t* event, common::Error err) override;

  virtual void HandleWindowResizeEvent(gui::events::WindowResizeEvent* event);
  virtual void HandleWindowExposeEvent(gui::events::WindowExposeEvent* event);
  virtual void HandleWindowCloseEvent(gui::events::WindowCloseEvent* event);

  virtual void HandleMouseStateChangeEvent(gui::events::MouseStateChangeEvent* event);
  virtual void HandleMousePressEvent(gui::events::MousePressEvent* event);
  virtual void HandleMouseMoveEvent(gui::events::MouseMoveEvent* event);
  virtual void HandleMouseReleaseEvent(events::MouseReleaseEvent* event);

  virtual void OnEnabledChanged(bool enable);
  virtual void OnFocusChanged(bool focus);
  virtual void OnMouseClicked(Uint8 button, const SDL_Point& position);
  virtual void OnMouseReleased(Uint8 button, const SDL_Point& position);

  bool IsPointInControlArea(const SDL_Point& point) const;

 private:
  void Init();

  Window* parent_;
  SDL_Rect rect_;
  SDL_Color back_ground_color_;
  SDL_Color border_color_;

  bool visible_;
  bool transparent_;
  bool bordered_;
  bool focus_;
  bool enabled_;
  common::draw::Size min_size_;

  mouse_clicked_callback_t mouse_clicked_cb_;
  mouse_released_callback_t mouse_released_cb_;
  focus_changed_callback_t focus_changed_cb_;
  enable_changed_callback_t enable_chaned_cb_;
};

}  // namespace gui

}  // namespace fastoplayer
