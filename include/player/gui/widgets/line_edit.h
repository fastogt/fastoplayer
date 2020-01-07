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

#include <string>

#include <common/time.h>

#include <player/gui/events/key_events.h>
#include <player/gui/widgets/label.h>

namespace fastoplayer {

namespace gui {

class LineEdit : public Label {
 public:
  typedef Label base_class;
  enum { blinking_cursor_time_msec = 500, cursor_width = 2 };

  explicit LineEdit(Window* parent = nullptr);
  explicit LineEdit(const SDL_Color& back_ground_color, Window* parent = nullptr);
  ~LineEdit() override;

  void SetPlaceHolder(const std::string& text);
  std::string GetPlaceHolder() const;

  bool IsActived() const;
  void SetActived(bool active);

  void Draw(SDL_Renderer* render) override;

 protected:
  void HandleEvent(event_t* event) override;
  void HandleExceptionEvent(event_t* event, common::Error err) override;

  void HandleMousePressEvent(gui::events::MousePressEvent* event) override;

  virtual void HandleKeyPressEvent(gui::events::KeyPressEvent* event);
  virtual void HandleKeyReleaseEvent(gui::events::KeyReleaseEvent* event);
  virtual void HandleTextInputEvent(gui::events::TextInputEvent* event);
  virtual void HandleTextEditEvent(gui::events::TextEditEvent* event);

  virtual void OnActiveChanged(bool active);

 private:
  void Init();
  bool active_;

  common::time64_t start_blink_ts_;
  bool show_cursor_;
  static LineEdit* last_actived_;
  std::string placeholder_;
};

}  // namespace gui

}  // namespace fastoplayer
