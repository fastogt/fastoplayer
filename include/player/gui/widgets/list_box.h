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
#include <vector>

#include <player/gui/widgets/font_window.h>

namespace fastoplayer {
namespace gui {

class IListBox : public FontWindow {
 public:
  typedef FontWindow base_class;
  typedef std::function<void(Uint8 button, size_t row)> mouse_clicked_row_callback_t;
  typedef std::function<void(Uint8 button, size_t row)> mouse_released_row_callback_t;

  enum Selection { NO_SELECT, SINGLE_ROW_SELECT };

  explicit IListBox(Window* parent = nullptr);
  explicit IListBox(const SDL_Color& back_ground_color, Window* parent = nullptr);
  ~IListBox() override;

  void SetMouseClickedRowCallback(mouse_clicked_row_callback_t cb);
  void SetMouseReeleasedRowCallback(mouse_clicked_row_callback_t cb);

  size_t GetMaxDrawRowCount() const;

  void SetSelection(Selection sel);
  Selection GetSelection() const;

  void SetSelectionColor(const SDL_Color& sel);
  SDL_Color GetSelectionColor() const;

  void SetActiveRowColor(const SDL_Color& sel);
  SDL_Color GetActiveRowColor() const;

  void SetActiveRow(size_t row);
  size_t GetActiveRow() const;

  void SetAlwaysActiveRowVisible(bool visible);
  bool GetAlwaysActiveRowVisible() const;

  void SetRowHeight(int row_height);
  int GetRowHeight() const;

  virtual size_t GetRowCount() const = 0;
  void Draw(SDL_Renderer* render) override;

 protected:
  virtual void DrawRow(SDL_Renderer* render, size_t pos, bool active, bool hover, const SDL_Rect& row_rect) = 0;
  void HandleMouseMoveEvent(gui::events::MouseMoveEvent* event) override;

  void OnFocusChanged(bool focus) override;
  void OnMouseClicked(Uint8 button, const SDL_Point& position) override;
  void OnMouseReleased(Uint8 button, const SDL_Point& position) override;

 private:
  size_t FindRowInPosition(const SDL_Point& position) const;

  int row_height_;
  size_t last_drawed_row_pos_;

  Selection selection_;
  SDL_Color selection_color_;
  size_t preselected_row_;

  SDL_Color active_row_color_;
  size_t active_row_position_;

  bool is_always_active_row_visible_;

  mouse_clicked_row_callback_t mouse_clicked_row_cb_;
  mouse_released_row_callback_t mouse_released_row_cb_;
};

class ListBox : public IListBox {
 public:
  typedef IListBox base_class;
  typedef std::vector<std::string> lines_t;

  ListBox();
  explicit ListBox(const SDL_Color& back_ground_color);
  ~ListBox() override;

  void SetLines(const lines_t& lines);
  lines_t GetLines() const;

  size_t GetRowCount() const override;

 protected:
  void DrawRow(SDL_Renderer* render, size_t pos, bool active, bool hover, const SDL_Rect& row_rect) override;

 private:
  lines_t lines_;
};

}  // namespace gui
}  // namespace fastoplayer
