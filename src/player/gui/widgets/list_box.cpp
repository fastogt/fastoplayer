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

#include <player/gui/widgets/list_box.h>

#include <player/draw/draw.h>

namespace fastoplayer {

namespace gui {

IListBox::IListBox(Window* parent)
    : base_class(parent),
      row_height_(0),
      last_drawed_row_pos_(0),
      selection_(NO_SELECT),
      selection_color_(),
      preselected_row_(draw::invalid_row_position),
      active_row_color_(),
      active_row_position_(draw::invalid_row_position),
      is_always_active_row_visible_(true),
      mouse_clicked_row_cb_(),
      mouse_released_row_cb_() {}

IListBox::IListBox(const SDL_Color& back_ground_color, Window* parent)
    : base_class(back_ground_color, parent),
      row_height_(0),
      last_drawed_row_pos_(0),
      selection_(NO_SELECT),
      selection_color_(),
      preselected_row_(draw::invalid_row_position),
      active_row_color_(),
      active_row_position_(draw::invalid_row_position),
      is_always_active_row_visible_(true),
      mouse_clicked_row_cb_(),
      mouse_released_row_cb_() {}

IListBox::~IListBox() {}

void IListBox::SetMouseClickedRowCallback(mouse_clicked_row_callback_t cb) {
  mouse_clicked_row_cb_ = cb;
}

void IListBox::SetMouseReeleasedRowCallback(mouse_clicked_row_callback_t cb) {
  mouse_released_row_cb_ = cb;
}

size_t IListBox::GetMaxDrawRowCount() const {
  SDL_Rect draw_area = GetRect();
  return draw_area.h / row_height_;
}

void IListBox::SetSelection(Selection sel) {
  selection_ = sel;
}

IListBox::Selection IListBox::GetSelection() const {
  return selection_;
}

void IListBox::SetSelectionColor(const SDL_Color& sel) {
  selection_color_ = sel;
}

SDL_Color IListBox::GetSelectionColor() const {
  return selection_color_;
}

void IListBox::SetActiveRowColor(const SDL_Color& sel) {
  active_row_color_ = sel;
}

SDL_Color IListBox::GetActiveRowColor() const {
  return active_row_color_;
}

void IListBox::SetActiveRow(size_t row) {
  active_row_position_ = row;
}

size_t IListBox::GetActiveRow() const {
  return active_row_position_;
}

void IListBox::SetAlwaysActiveRowVisible(bool visible) {
  is_always_active_row_visible_ = visible;
}

bool IListBox::GetAlwaysActiveRowVisible() const {
  return is_always_active_row_visible_;
}

void IListBox::SetRowHeight(int row_height) {
  row_height_ = row_height;
}

int IListBox::GetRowHeight() const {
  return row_height_;
}

void IListBox::Draw(SDL_Renderer* render) {
  if (!IsCanDraw()) {
    return;
  }

  base_class::Draw(render);

  if (row_height_ <= 0) {  // nothing to draw, prevent devide by zero
    DNOTREACHED();
    return;
  }

  size_t max_line_count = GetMaxDrawRowCount();
  if (max_line_count == 0) {  // nothing to draw
    return;
  }

  SDL_Rect draw_area = GetRect();
  if (active_row_position_ != draw::invalid_row_position) {
    if (active_row_position_ >= max_line_count) {
      last_drawed_row_pos_ = active_row_position_ - max_line_count + 1;
    } else {
      last_drawed_row_pos_ = 0;
    }
  }

  size_t drawed = 0;
  for (size_t i = last_drawed_row_pos_; i < GetRowCount() && drawed < max_line_count; ++i) {
    SDL_Rect row_rect = {draw_area.x, draw_area.y + row_height_ * static_cast<int>(drawed), draw_area.w, row_height_};
    bool hover_row = preselected_row_ == i;
    bool is_active_row = active_row_position_ == i;
    DrawRow(render, i, is_active_row, hover_row, row_rect);
    if (hover_row && selection_ == SINGLE_ROW_SELECT) {
      common::Error err = draw::FillRectColor(render, row_rect, selection_color_);
      DCHECK(!err) << err->GetDescription();
    }
    if (is_active_row) {
      common::Error err = draw::FillRectColor(render, row_rect, active_row_color_);
      DCHECK(!err) << err->GetDescription();
    }
    drawed++;
  }
}

void IListBox::HandleMouseMoveEvent(gui::events::MouseMoveEvent* event) {
  gui::events::MouseMoveInfo minf = event->GetInfo();
  SDL_Point point = minf.GetMousePoint();
  preselected_row_ = FindRowInPosition(point);
  base_class::HandleMouseMoveEvent(event);
}

void IListBox::OnFocusChanged(bool focus) {
  if (!focus) {
    preselected_row_ = draw::invalid_row_position;
  }

  base_class::OnFocusChanged(focus);
}

void IListBox::OnMouseClicked(Uint8 button, const SDL_Point& position) {
  size_t pos = FindRowInPosition(position);
  if (pos != draw::invalid_row_position) {
    if (mouse_clicked_row_cb_) {
      mouse_clicked_row_cb_(button, pos);
    }
    active_row_position_ = pos;
  }

  base_class::OnMouseClicked(button, position);
}

void IListBox::OnMouseReleased(Uint8 button, const SDL_Point& position) {
  size_t pos = FindRowInPosition(position);
  if (pos != draw::invalid_row_position) {
    if (mouse_released_row_cb_) {
      mouse_released_row_cb_(button, pos);
    }
    active_row_position_ = pos;
  }

  base_class::OnMouseReleased(button, position);
}

size_t IListBox::FindRowInPosition(const SDL_Point& position) const {
  if (!IsCanDraw()) {
    return draw::invalid_row_position;
  }

  if (!IsPointInControlArea(position)) {  // not in rect
    return draw::invalid_row_position;
  }

  if (row_height_ <= 0) {  // nothing to draw, prevent devide by zero
    DNOTREACHED();
    return draw::invalid_row_position;
  }

  SDL_Rect draw_area = GetRect();
  size_t max_line_count = GetMaxDrawRowCount();
  if (max_line_count == 0) {  // nothing draw
    return draw::invalid_row_position;
  }

  size_t drawed = 0;
  for (size_t i = last_drawed_row_pos_; i < GetRowCount() && drawed < max_line_count; ++i) {
    SDL_Rect cell_rect = {draw_area.x, draw_area.y + row_height_ * static_cast<int>(drawed), draw_area.w, row_height_};
    if (draw::IsPointInRect(position, cell_rect)) {
      return i;
    }
    drawed++;
  }

  return draw::invalid_row_position;
}

ListBox::ListBox() : base_class(), lines_() {}

ListBox::ListBox(const SDL_Color& back_ground_color) : base_class(back_ground_color), lines_() {}

ListBox::~ListBox() {}

void ListBox::SetLines(const lines_t& lines) {
  lines_ = lines;
}

ListBox::lines_t ListBox::GetLines() const {
  return lines_;
}

size_t ListBox::GetRowCount() const {
  return lines_.size();
}

void ListBox::DrawRow(SDL_Renderer* render, size_t pos, bool active, bool hover, const SDL_Rect& row_rect) {
  UNUSED(active);
  UNUSED(hover);
  DrawText(render, lines_[pos], row_rect, GetDrawType());
}

}  // namespace gui

}  // namespace fastoplayer
