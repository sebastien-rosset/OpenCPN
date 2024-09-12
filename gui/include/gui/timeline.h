/******************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  Timeline Widget
 * Author:   Sebastien Rosset
 *
 ***************************************************************************
 *   Copyright (C) 2024 by Sebastien Rosset                                *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA.         *
 ****************************************************************************/

#ifndef TIMELINE_H
#define TIMELINE_H

#include <wx/wx.h>
#include <wx/datetime.h>
#include <vector>
#include <memory>

#include "icon_button.h"

/**
 * Represents an element on the timeline.
 */
struct TimelineElement {
  /**
   * Represents a part of a timeline element.
   */
  struct Part {
    wxDateTime start;     ///< Start time of the part
    wxTimeSpan duration;  ///< Duration of the part
    wxColour color;       ///< Color of the part
    wxString label;       ///< Label for the part
  };

  std::vector<Part> parts;  ///< Vector of parts composing the timeline element
};

/**
 * A custom widget for displaying a timeline.
 *
 * A timeline can display various elements over a specified time range.
 * It includes controls for zooming, playing, and navigating through the
 * timeline.
 */
class Timeline : public wxPanel {
public:
  /**
   * @brief Construct a new Timeline object
   *
   * @param parent The parent window
   * @param start The start date/time for the timeline
   * @param timelineDuration The duration of the timeline
   */
  Timeline(wxWindow* parent,
           const wxDateTime& timelineStartTs = wxDateTime::Now(),
           const wxTimeSpan& timelineDuration = wxTimeSpan::Days(16));

  ~Timeline();

  /**
   * @brief Set the timeline elements to be displayed
   *
   * @param elements Vector of TimelineElement objects to display
   */
  void SetTimelineElements(const std::vector<TimelineElement>& elements);

private:
  // GUI components
  wxStaticText* m_timestamp_label;
  IconButton* m_play_button;  //< Button to play/pause the timeline.
  IconButton* m_now_button;   //< Button to set the focus time to the current
                              // computer time.
  IconButton*
      m_previous_button;      //< Button to move the focus time backward (left).
  IconButton* m_next_button;  //< Button to move the focus time forward (right).
  IconButton* m_zoom_in_button;   //< Button to zoom in the timeline.
  IconButton* m_zoom_out_button;  //< Button to zoom out the timeline.
  wxPanel* m_timeline_panel;
  wxBoxSizer* m_ctrl_panel;

  /**
   * The start time of the timeline.
   * The user may adjust the start time by zooming in/out or moving the
   * timeline.
   */
  wxDateTime m_start_timestamp;
  /**
   * The duration of the timeline.
   * end time = m_start_timestamp + m_timeline_duration
   */
  wxTimeSpan m_timeline_duration;
  /**
   * The current focus time.
   * This is the selected time on the timeline, which may be adjusted by the
   * user by dragging the vertical line representing the focus time, or when
   * playing the timeline.
   */
  wxDateTime m_selectedTimestamp;
  std::vector<TimelineElement> m_timelineElements;

  // Member variables to control the timeline (play, zoom, drag)
  bool m_isPlaying;   //< Flag indicating if the timeline is currently playing.
  bool m_isDragging;  //< Flag indicating if the user is currently dragging the
                      // timeline.
  int m_dragStartX;   //< X-coordinate where the drag operation started.
  wxTimer* m_play_timer;    //< Timer to increment the focusTime when playing.
  wxTimer* m_update_timer;  //< Timer to update the vertical line representing
                            // the current computer time.
  wxTimeSpan m_update_interval;  //< Interval for updating the timeline display.

  // Event handlers
  void OnPaint(wxPaintEvent& event);
  void OnSize(wxSizeEvent& event);
  void OnPlayPause(wxCommandEvent& event);
  void OnZoomIn(wxCommandEvent& event);
  void OnZoomOut(wxCommandEvent& event);
  void OnNowButton(wxCommandEvent& event);
  void OnMoveBackward(wxCommandEvent& event);
  void OnMoveForward(wxCommandEvent& event);
  void OnMouseDown(wxMouseEvent& event);
  void OnMouseMove(wxMouseEvent& event);
  void OnMouseUp(wxMouseEvent& event);
  void OnPlayTimer(wxTimerEvent& event);
  void OnUpdateTimer(wxTimerEvent& event);

  // Helper methods
  void DrawTimeline(wxDC& dc);
  void DrawTimeUnits(wxDC& dc, const wxString& timeUnit, int interval);
  void DrawTimelineElements(wxDC& dc);
  /** Draw a vertical line representing the specified timestamp on the timeline.
   */
  void DrawTimeIndicator(wxDC& dc, const wxDateTime& timestamp,
                         const wxColour& color = wxColour(0, 0, 0));

  void UpdateTimelineDisplay();
  wxString FormatTimestamp(const wxDateTime& timestamp) const;
  std::pair<wxString, int> GetTimeUnitAndInterval() const;
  wxString FormatDate(const wxDateTime& date, const wxString& timeUnit,
                      int availableWidth = 0) const;
  void EnsureTimestampVisible();

  // Returns the duration of the timeline increment for each "play" timer tick.
  // The goal is to traverse the entire timeline in about 60 seconds when
  // playing.
  wxTimeSpan CalculateTimeIncrement() const;
};

#endif  // TIMELINE_H
