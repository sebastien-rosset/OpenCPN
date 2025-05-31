/***************************************************************************
 *   Copyright (C) 2024 by OpenCPN development team                        *
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
 **************************************************************************/

#include "timeline.h"
#include <wx/graphics.h>
#include <wx/dcbuffer.h>
#include "styles.h"
#include "svg_utils.h"
#include "OCPNPlatform.h"

extern ocpnStyle::StyleManager* g_StyleManager;
extern OCPNPlatform* g_Platform;

Timeline::Timeline(wxWindow* parent, const wxDateTime& timelineStartTs,
                   const wxTimeSpan& timelineDuration)
    : wxPanel(parent),
      m_startTimestamp(timelineStartTs),
      m_timelineDuration(timelineDuration),
      m_selectedTimestamp(timelineStartTs),
      m_isDragging(false),
      m_isDraggingIndicator(false),
      m_isDraggingTimeline(false),
      m_dragStartX(0),
      m_dragOffsetX(0) {
  m_isPlaying = false;
  m_playTimer = new wxTimer(this);
  m_updateTimer = new wxTimer(this);
  m_updateInterval = wxTimeSpan::Milliseconds(250);

  // Set background to match OpenCPN's UI theme
  SetBackgroundColour(wxColour(240, 240, 240));  // Light grey background

  // Create GUI components with better sizing
  m_timestampLabel =
      new wxStaticText(this, wxID_ANY, FormatTimestamp(m_selectedTimestamp));
  wxFont font = m_timestampLabel->GetFont();
  font.SetPointSize(10);
  font.SetWeight(wxFONTWEIGHT_BOLD);
  m_timestampLabel->SetFont(font);

  // Create OpenCPN-style bitmap buttons
  int buttonSize = 32;

  // Load timeline-specific SVG icons
  wxString iconDir = g_Platform->GetSharedDataDir() + "uidata/timeline_icons/";

  m_playButton = new wxBitmapButton(
      this, wxID_ANY, LoadSVG(iconDir + "play.svg", buttonSize, buttonSize),
      wxDefaultPosition, wxSize(buttonSize, buttonSize));
  m_nowButton = new wxBitmapButton(
      this, wxID_ANY, LoadSVG(iconDir + "now.svg", buttonSize, buttonSize),
      wxDefaultPosition, wxSize(buttonSize, buttonSize));
  m_leftButton = new wxBitmapButton(
      this, wxID_ANY, LoadSVG(iconDir + "prev.svg", buttonSize, buttonSize),
      wxDefaultPosition, wxSize(buttonSize, buttonSize));
  m_rightButton = new wxBitmapButton(
      this, wxID_ANY, LoadSVG(iconDir + "next.svg", buttonSize, buttonSize),
      wxDefaultPosition, wxSize(buttonSize, buttonSize));
  m_zoomInButton = new wxBitmapButton(
      this, wxID_ANY, LoadSVG(iconDir + "zoom_in.svg", buttonSize, buttonSize),
      wxDefaultPosition, wxSize(buttonSize, buttonSize));
  m_zoomOutButton = new wxBitmapButton(
      this, wxID_ANY, LoadSVG(iconDir + "zoom_out.svg", buttonSize, buttonSize),
      wxDefaultPosition, wxSize(buttonSize, buttonSize));

  // Add tooltips for better user experience
  m_playButton->SetToolTip(_("Play/Stop timeline animation"));
  m_nowButton->SetToolTip(_("Jump to current time"));
  m_leftButton->SetToolTip(_("Move backward in time"));
  m_rightButton->SetToolTip(_("Move forward in time"));
  m_zoomInButton->SetToolTip(_("Zoom in timeline"));
  m_zoomOutButton->SetToolTip(_("Zoom out timeline"));

  m_timelinePanel = new wxPanel(this, wxID_ANY, wxDefaultPosition,
                                wxDefaultSize, wxBORDER_SUNKEN);
  m_timelinePanel->SetMinSize(wxSize(-1, 35));  // Reduced from 45 to 35
  m_timelinePanel->SetMaxSize(wxSize(-1, 35));
  m_timelinePanel->SetBackgroundColour(
      wxColour(255, 255, 255));  // White timeline area

  // Bind events
  Bind(wxEVT_PAINT, &Timeline::OnPaint, this);
  Bind(wxEVT_SIZE, &Timeline::OnSize, this);
  m_playTimer->Bind(wxEVT_TIMER, &Timeline::OnPlayTimer, this);
  m_updateTimer->Bind(wxEVT_TIMER, &Timeline::OnUpdateTimer, this);

  m_playButton->Bind(wxEVT_BUTTON, &Timeline::OnPlayPause, this);
  m_nowButton->Bind(wxEVT_BUTTON, &Timeline::OnNowButton, this);
  m_leftButton->Bind(wxEVT_BUTTON, &Timeline::OnMoveBackward, this);
  m_rightButton->Bind(wxEVT_BUTTON, &Timeline::OnMoveForward, this);
  m_zoomInButton->Bind(wxEVT_BUTTON, &Timeline::OnZoomIn, this);
  m_zoomOutButton->Bind(wxEVT_BUTTON, &Timeline::OnZoomOut, this);

  m_timelinePanel->Bind(wxEVT_PAINT, &Timeline::OnPaint, this);
  m_timelinePanel->Bind(wxEVT_LEFT_DOWN, &Timeline::OnMouseDown, this);
  m_timelinePanel->Bind(wxEVT_MOTION, &Timeline::OnMouseMove, this);
  m_timelinePanel->Bind(wxEVT_LEFT_UP, &Timeline::OnMouseUp, this);
  m_timelinePanel->Bind(wxEVT_LEAVE_WINDOW, &Timeline::OnMouseLeave, this);

  // Layout with proper spacing
  m_ctrlPanel = new wxBoxSizer(wxHORIZONTAL);
  m_ctrlPanel->Add(m_playButton, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
  m_ctrlPanel->Add(m_timelinePanel, 1, wxEXPAND | wxLEFT | wxRIGHT, 4);
  m_ctrlPanel->Add(m_zoomOutButton, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);
  m_ctrlPanel->Add(m_zoomInButton, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 2);
  m_ctrlPanel->Add(m_nowButton, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);
  m_ctrlPanel->Add(m_leftButton, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 2);
  m_ctrlPanel->Add(m_rightButton, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 2);

  wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
  mainSizer->Add(m_timestampLabel, 0, wxALIGN_CENTER_HORIZONTAL | wxTOP, 3);
  mainSizer->Add(m_ctrlPanel, 0, wxEXPAND | wxALL, 4);

  SetSizer(mainSizer);

  // If selected timestamp is current time, position optimally for weather
  // forecasting
  wxDateTime now = wxDateTime::Now();
  if (abs((m_selectedTimestamp - now).GetSeconds().ToLong()) < 60) {
    // Selected time is within 1 minute of current time, so position optimally
    CenterCurrentTimeOptimally();
  }

  m_updateTimer->Start(60000);  // Update every minute
}

Timeline::~Timeline() {
  delete m_playTimer;
  delete m_updateTimer;
}

void Timeline::SetTimelineElements(
    const std::vector<TimelineElement>& elements) {
  m_timelineElements = elements;
  m_timelinePanel->Refresh();
}

void Timeline::OnPaint(wxPaintEvent& event) {
  wxAutoBufferedPaintDC dc(m_timelinePanel);
  dc.Clear();
  DrawTimeline(dc);
}

void Timeline::OnSize(wxSizeEvent& event) {
  m_timelinePanel->Refresh();
  event.Skip();
}

void Timeline::DrawTimeline(wxDC& dc) {
  wxSize size = m_timelinePanel->GetSize();
  int width = size.GetWidth();
  int height = size.GetHeight();

  // Draw time units
  wxString timeUnit;
  int interval;
  std::tie(timeUnit, interval) = GetTimeUnitAndInterval();
  DrawTimeUnits(dc, timeUnit, interval);

  // Draw timeline elements
  DrawTimelineElements(dc);

  DrawTimeIndicator(dc, m_selectedTimestamp);

  /** Draw "now" indicator if it's within the visible range.  */
  wxDateTime now = wxDateTime::Now();
  if (m_startTimestamp <= now && now <= m_startTimestamp + m_timelineDuration) {
    DrawTimeIndicator(dc, now, wxColour(0, 0, 200));  // Blue for "now"
  }
}

void Timeline::DrawTimeUnits(wxDC& dc, const wxString& timeUnit, int interval) {
  wxSize size = m_timelinePanel->GetSize();
  int width = size.GetWidth();
  int height = size.GetHeight();

  // Calculate the effective timeline range for drawing
  // During drag, extend the range to cover the visual offset area
  wxDateTime effectiveStart = m_startTimestamp;
  wxDateTime effectiveEnd = m_startTimestamp + m_timelineDuration;

  if (m_isDraggingTimeline) {
    // Direction-aware range extension based on drag direction
    double offsetFraction = static_cast<double>(m_dragOffsetX) / width;
    wxTimeSpan offsetTime = wxTimeSpan::Seconds(
        m_timelineDuration.GetSeconds().ToLong() * abs(offsetFraction));

    if (m_dragOffsetX > 0) {
      // Right drag (positive offset): extend left side to show content sliding
      // in from left
      effectiveStart = m_startTimestamp - offsetTime;
      effectiveEnd = m_startTimestamp + m_timelineDuration;
    } else if (m_dragOffsetX < 0) {
      // Left drag (negative offset): extend right side to show content sliding
      // in from right
      effectiveStart = m_startTimestamp;
      effectiveEnd = m_startTimestamp + m_timelineDuration + offsetTime;
    }
    // If m_dragOffsetX == 0, use original range (no extension needed)
  }

  wxDateTime current = effectiveStart;

  // Always align to proper time boundaries for clean display
  // This ensures times show as 07:00, 08:00, etc. instead of 07:23, 08:23
  if (timeUnit == "hour") {
    // Align to hour boundary
    current = wxDateTime(current.GetDay(), current.GetMonth(),
                         current.GetYear(), current.GetHour(), 0, 0, 0);
  } else if (timeUnit == "day") {
    // Align to day boundary
    current = wxDateTime(current.GetDay(), current.GetMonth(),
                         current.GetYear(), 0, 0, 0, 0);
  } else if (timeUnit == "month") {
    // Align to month boundary
    current = wxDateTime(1, current.GetMonth(), current.GetYear(), 0, 0, 0, 0);
  } else if (timeUnit == "year") {
    // Align to year boundary
    current = wxDateTime(1, wxDateTime::Jan, current.GetYear(), 0, 0, 0, 0);
  }

  // For right drags during dragging, we may need to step back further
  // to ensure we generate enough content for the extended range
  if (m_isDraggingTimeline && m_dragOffsetX > 0) {
    // Right drag: step back one more interval to ensure complete coverage
    if (timeUnit == "hour") {
      current -= wxTimeSpan::Hours(1);
    } else if (timeUnit == "day") {
      current -= wxDateSpan::Days(1);
    } else if (timeUnit == "month") {
      current -= wxDateSpan::Months(1);
    } else if (timeUnit == "year") {
      current -= wxDateSpan::Years(1);
    }
  }

  // Draw major time divisions
  while (current <= effectiveEnd) {
    double fraction = (current - m_startTimestamp).GetSeconds().ToDouble() /
                      m_timelineDuration.GetSeconds().ToDouble();
    int x = static_cast<int>(fraction * width);

    // Apply drag offset for smooth panning during timeline drag
    if (m_isDraggingTimeline) {
      x += m_dragOffsetX;
    }

    // Only draw if the line would be visible in the panel
    if (x >= -50 && x <= width + 50) {  // Small margin for smooth transitions
      // Draw major vertical line to mark start/end of time period
      dc.SetPen(wxPen(wxColour(160, 160, 160),
                      2));  // Darker grey for major divisions
      dc.DrawLine(x, 18, x, height);

      wxString label = FormatDate(current, timeUnit);
      wxSize textSize = dc.GetTextExtent(label);

      dc.SetTextForeground(wxColour(100, 100, 100));
      dc.DrawText(label, x - textSize.GetWidth() / 2, 0);
    }

    if (timeUnit == "hour")
      current += wxTimeSpan::Hours(interval);
    else if (timeUnit == "day")
      current += wxDateSpan::Days(interval);
    else if (timeUnit == "month")
      current += wxDateSpan::Months(interval);
    else if (timeUnit == "year")
      current += wxDateSpan::Years(interval);
  }

  // Draw minor subdivisions for better granularity
  DrawTimeSubdivisions(dc, timeUnit, interval);
}

void Timeline::DrawTimeSubdivisions(wxDC& dc, const wxString& timeUnit,
                                    int interval) {
  wxSize size = m_timelinePanel->GetSize();
  int width = size.GetWidth();
  int height = size.GetHeight();

  // Calculate the effective timeline range for drawing
  wxDateTime effectiveStart = m_startTimestamp;
  wxDateTime effectiveEnd = m_startTimestamp + m_timelineDuration;

  if (m_isDraggingTimeline) {
    // Direction-aware range extension based on drag direction
    double offsetFraction = static_cast<double>(m_dragOffsetX) / width;
    wxTimeSpan offsetTime = wxTimeSpan::Seconds(
        m_timelineDuration.GetSeconds().ToLong() * abs(offsetFraction));

    if (m_dragOffsetX > 0) {
      // Right drag (positive offset): extend left side to show content sliding
      // in from left
      effectiveStart = m_startTimestamp - offsetTime;
      effectiveEnd = m_startTimestamp + m_timelineDuration;
    } else if (m_dragOffsetX < 0) {
      // Left drag (negative offset): extend right side to show content sliding
      // in from right
      effectiveStart = m_startTimestamp;
      effectiveEnd = m_startTimestamp + m_timelineDuration + offsetTime;
    }
    // If m_dragOffsetX == 0, use original range (no extension needed)
  }

  wxDateTime current = effectiveStart;

  // Always align to proper time boundaries for clean subdivision display
  // This ensures subdivisions are positioned correctly relative to main
  // divisions
  if (timeUnit == "hour") {
    // Align to hour boundary
    current = wxDateTime(current.GetDay(), current.GetMonth(),
                         current.GetYear(), current.GetHour(), 0, 0, 0);
  } else if (timeUnit == "day") {
    // Align to day boundary
    current = wxDateTime(current.GetDay(), current.GetMonth(),
                         current.GetYear(), 0, 0, 0, 0);
  } else if (timeUnit == "month") {
    // Align to month boundary
    current = wxDateTime(1, current.GetMonth(), current.GetYear(), 0, 0, 0, 0);
  }

  // For right drags during dragging, step back to ensure complete coverage
  if (m_isDraggingTimeline && m_dragOffsetX > 0) {
    if (timeUnit == "hour") {
      current -= wxTimeSpan::Hours(1);
    } else if (timeUnit == "day") {
      current -= wxDateSpan::Days(1);
    } else if (timeUnit == "month") {
      current -= wxDateSpan::Months(1);
    }
  }

  int subDivisions = 1;

  // Determine subdivision interval based on main time unit
  if (timeUnit == "hour") {
    subDivisions = 4;  // 15-minute subdivisions
  } else if (timeUnit == "day") {
    subDivisions = 4;  // 6-hour subdivisions
  } else if (timeUnit == "month") {
    subDivisions = 4;  // Weekly subdivisions
  } else {
    // For years and other units, don't add subdivisions to avoid clutter
    return;
  }

  // Draw subdivisions
  while (current <= effectiveEnd) {
    for (int i = 1; i < subDivisions; i++) {
      wxDateTime subTime = current;

      // Calculate subdivision time based on unit type
      if (timeUnit == "hour") {
        subTime += wxTimeSpan::Minutes(15 * i);
      } else if (timeUnit == "day") {
        subTime += wxTimeSpan::Hours(6 * i);
      } else if (timeUnit == "month") {
        subTime += wxDateSpan::Weeks(i);
      }

      if (subTime > effectiveEnd) break;

      double fraction = (subTime - m_startTimestamp).GetSeconds().ToDouble() /
                        m_timelineDuration.GetSeconds().ToDouble();
      int x = static_cast<int>(fraction * width);

      // Apply drag offset for smooth panning during timeline drag
      if (m_isDraggingTimeline) {
        x += m_dragOffsetX;
      }

      // Only draw if the line would be visible in the panel
      if (x >= -50 && x <= width + 50) {
        // Draw minor subdivision line
        dc.SetPen(wxPen(wxColour(200, 200, 200),
                        1));  // Very light grey for minor divisions
        dc.DrawLine(x, height - 10, x, height);  // Short lines at bottom
      }
    }

    if (timeUnit == "hour")
      current += wxTimeSpan::Hours(interval);
    else if (timeUnit == "day")
      current += wxDateSpan::Days(interval);
    else if (timeUnit == "month")
      current += wxDateSpan::Months(interval);
  }
}

void Timeline::DrawTimelineElements(wxDC& dc) {
  wxSize size = m_timelinePanel->GetSize();
  int width = size.GetWidth();
  int height = size.GetHeight();

  int elementHeight = 8;
  int elementPadding = 2;
  int startY = 20;  // Start below the time unit labels

  for (size_t i = 0; i < m_timelineElements.size(); ++i) {
    int elementY = startY + i * (elementHeight + elementPadding);

    for (const auto& part : m_timelineElements[i].parts) {
      double startFraction =
          (part.start - m_startTimestamp).GetSeconds().ToDouble() /
          m_timelineDuration.GetSeconds().ToDouble();
      double endFraction = (part.start + part.duration - m_startTimestamp)
                               .GetSeconds()
                               .ToDouble() /
                           m_timelineDuration.GetSeconds().ToDouble();

      int startX = static_cast<int>(startFraction * width);
      int endX = static_cast<int>(endFraction * width);

      // Apply drag offset for smooth panning during timeline drag
      if (m_isDraggingTimeline) {
        startX += m_dragOffsetX;
        endX += m_dragOffsetX;
      }

      // Only draw if the element would be visible in the panel (with margin)
      if (endX >= -50 && startX <= width + 50) {
        dc.SetPen(wxPen(part.color, elementHeight));
        dc.DrawLine(startX, elementY + elementHeight / 2, endX,
                    elementY + elementHeight / 2);
      }
    }
  }
}

void Timeline::DrawTimeIndicator(wxDC& dc, const wxDateTime& timestamp,
                                 const wxColour& color) {
  wxSize size = m_timelinePanel->GetSize();
  int width = size.GetWidth();
  int height = size.GetHeight();

  double fraction = (timestamp - m_startTimestamp).GetSeconds().ToDouble() /
                    m_timelineDuration.GetSeconds().ToDouble();
  int x = static_cast<int>(fraction * width);

  // Apply drag offset for smooth panning during timeline drag
  // Note: Only apply to non-selected time indicators during timeline drag
  if (m_isDraggingTimeline && timestamp != m_selectedTimestamp) {
    x += m_dragOffsetX;
  }

  // Only draw if the indicator would be visible in the panel (with margin)
  if (x >= -50 && x <= width + 50) {
    dc.SetPen(wxPen(color, 2));
    dc.DrawLine(x, 0, x, height);

    // Draw triangles at top and bottom
    wxPoint points[3];
    points[0] = wxPoint(x, 0);
    points[1] = wxPoint(x - 3, 6);
    points[2] = wxPoint(x + 3, 6);
    dc.SetBrush(wxBrush(color));
    dc.DrawPolygon(3, points);

    points[0] = wxPoint(x, height);
    points[1] = wxPoint(x - 3, height - 6);
    points[2] = wxPoint(x + 3, height - 6);
    dc.DrawPolygon(3, points);
  }  // End visibility check
}

void Timeline::OnPlayPause(wxCommandEvent& event) {
  m_isPlaying = !m_isPlaying;
  wxString iconDir = g_Platform->GetSharedDataDir() + "uidata/timeline_icons/";
  int buttonSize = 32;

  if (m_isPlaying) {
    m_playButton->SetBitmapLabel(
        LoadSVG(iconDir + "stop.svg", buttonSize, buttonSize));
    m_playTimer->Start(m_updateInterval.GetMilliseconds().GetLo());
  } else {
    m_playButton->SetBitmapLabel(
        LoadSVG(iconDir + "play.svg", buttonSize, buttonSize));
    m_playTimer->Stop();
  }
}

void Timeline::OnPlayTimer(wxTimerEvent& event) {
  wxTimeSpan increment = CalculateTimeIncrement();
  m_selectedTimestamp += increment;
  m_timestampLabel->SetLabel(FormatTimestamp(m_selectedTimestamp));
  m_timelinePanel->Refresh();

  if (m_selectedTimestamp >= m_startTimestamp + m_timelineDuration) {
    m_playTimer->Stop();
    m_isPlaying = false;
    wxString iconDir =
        g_Platform->GetSharedDataDir() + "uidata/timeline_icons/";
    m_playButton->SetBitmapLabel(LoadSVG(iconDir + "play.svg", 32, 32));
  }
}

wxTimeSpan Timeline::CalculateTimeIncrement() const {
  wxTimeSpan targetTraversalTime = wxTimeSpan::Seconds(60);
  double traversalTicks = targetTraversalTime.GetMilliseconds().ToDouble() /
                          m_updateInterval.GetMilliseconds().ToDouble();
  wxLongLong milliseconds = m_timelineDuration.GetMilliseconds() /
                            static_cast<long long>(traversalTicks);
  return wxTimeSpan::Milliseconds(milliseconds);
}

void Timeline::OnZoomIn(wxCommandEvent& event) {
  if (m_timelineDuration > wxTimeSpan::Hours(4)) {
    m_timelineDuration = wxTimeSpan::Milliseconds(
        m_timelineDuration.GetMilliseconds().GetValue() / 2);
    UpdateTimelineDisplay();
  }
}

void Timeline::OnZoomOut(wxCommandEvent& event) {
  if (m_timelineDuration < wxTimeSpan::Days(365 * 4)) {
    m_timelineDuration = wxTimeSpan::Milliseconds(
        m_timelineDuration.GetMilliseconds().GetValue() * 2);
    UpdateTimelineDisplay();
  }
}

void Timeline::OnNowButton(wxCommandEvent& event) {
  m_selectedTimestamp = wxDateTime::Now();
  CenterCurrentTimeOptimally();
  UpdateTimelineDisplay();
}

void Timeline::CenterCurrentTimeOptimally() {
  // Position current time at 1/4 from the left for optimal weather viewing
  // This provides some look-back context while maximizing look-ahead time

  // Calculate how much time should be to the left of current time (25% of
  // duration)
  wxTimeSpan lookBackTime =
      wxTimeSpan::Seconds(m_timelineDuration.GetSeconds().ToLong() / 4);

  // Set the timeline start to position current time at the 1/4 mark
  m_startTimestamp = m_selectedTimestamp - lookBackTime;
}

void Timeline::OnUpdateTimer(wxTimerEvent& event) {
  // Update the display to show current time indicator
  m_timelinePanel->Refresh();
}

void Timeline::OnMoveBackward(wxCommandEvent& event) {
  wxString timeUnit;
  int interval;
  std::tie(timeUnit, interval) = GetTimeUnitAndInterval();

  if (timeUnit == "hour")
    m_selectedTimestamp -= wxTimeSpan::Hours(interval);
  else if (timeUnit == "day")
    m_selectedTimestamp -= wxDateSpan::Days(interval);
  else if (timeUnit == "month")
    m_selectedTimestamp -= wxDateSpan::Months(interval);
  else if (timeUnit == "year")
    m_selectedTimestamp -= wxDateSpan::Years(interval);

  EnsureTimestampVisible();
  UpdateTimelineDisplay();
}

void Timeline::OnMoveForward(wxCommandEvent& event) {
  wxString timeUnit;
  int interval;
  std::tie(timeUnit, interval) = GetTimeUnitAndInterval();

  if (timeUnit == "hour")
    m_selectedTimestamp += wxTimeSpan::Hours(interval);
  else if (timeUnit == "day")
    m_selectedTimestamp += wxDateSpan::Days(interval);
  else if (timeUnit == "month")
    m_selectedTimestamp += wxDateSpan::Months(interval);
  else if (timeUnit == "year")
    m_selectedTimestamp += wxDateSpan::Years(interval);

  EnsureTimestampVisible();
  UpdateTimelineDisplay();
}

void Timeline::OnMouseDown(wxMouseEvent& event) {
  if (m_isPlaying) {
    wxCommandEvent dummyEvent;
    OnPlayPause(dummyEvent);
  }

  m_isDragging = true;
  m_dragStartX = event.GetX();
  m_dragOffsetX = 0;  // Reset drag offset
  m_dragStartTimestamp = m_selectedTimestamp;
  m_dragStartTimelineStart = m_startTimestamp;

  // Determine what the user clicked on
  if (IsClickOnTimeIndicator(event.GetX())) {
    m_isDraggingIndicator = true;
    m_isDraggingTimeline = false;
    // Change cursor to indicate we're dragging the time indicator
    m_timelinePanel->SetCursor(wxCursor(wxCURSOR_SIZEWE));
  } else {
    m_isDraggingIndicator = false;
    m_isDraggingTimeline = true;
    // Change cursor to indicate we're panning the timeline
    m_timelinePanel->SetCursor(wxCursor(wxCURSOR_HAND));
  }
}

void Timeline::OnMouseMove(wxMouseEvent& event) {
  if (!m_isDragging) {
    // Update cursor based on what's under the mouse when not dragging
    if (IsClickOnTimeIndicator(event.GetX())) {
      m_timelinePanel->SetCursor(wxCursor(wxCURSOR_SIZEWE));
    } else {
      m_timelinePanel->SetCursor(wxCursor(wxCURSOR_HAND));
    }
    return;
  }

  // Use consistent coordinate system - all in logical pixels
  int currentX = event.GetX();       // Logical pixels from wxMouseEvent
  int dx = currentX - m_dragStartX;  // Logical pixel difference
  wxSize panelSize = m_timelinePanel->GetSize();  // Logical pixel size
  int panelWidth = panelSize.GetWidth();          // Logical pixels

  // Debug output for investigating speed mismatch (can be enabled for
  // debugging) wxLogDebug("Mouse Move: currentX=%d, dragStartX=%d, dx=%d,
  // panelWidth=%d, offsetX=%d",
  //           currentX, m_dragStartX, dx, panelWidth, m_dragOffsetX);

  if (m_isDraggingIndicator) {
    // Dragging the time indicator: change the selected timestamp
    double fraction = static_cast<double>(dx) / panelWidth;
    wxTimeSpan delta = wxTimeSpan::Seconds(
        m_timelineDuration.GetSeconds().ToLong() * fraction);
    m_selectedTimestamp = m_dragStartTimestamp + delta;
    EnsureTimestampVisible();
    UpdateTimelineDisplay();
  } else if (m_isDraggingTimeline) {
    // Dragging the timeline background: store raw pixel offset
    m_dragOffsetX = dx;

    // Update the timestamp label to show the time at the indicator position
    double fraction = static_cast<double>(-dx) / panelWidth;
    wxTimeSpan delta = wxTimeSpan::Seconds(
        m_timelineDuration.GetSeconds().ToLong() * fraction);
    wxDateTime newSelectedTime = m_dragStartTimestamp + delta;
    m_timestampLabel->SetLabel(FormatTimestamp(newSelectedTime));

    // Refresh the timeline panel to show the visual offset
    m_timelinePanel->Refresh();
  }
}

void Timeline::OnMouseUp(wxMouseEvent& event) {
  if (m_isDraggingTimeline && m_dragOffsetX != 0) {
    // Commit the timeline drag operation using consistent calculation
    wxSize panelSize = m_timelinePanel->GetSize();
    int panelWidth = panelSize.GetWidth();

    // Use the final mouse position for exact calculation
    int finalDx = event.GetX() - m_dragStartX;
    double fraction = static_cast<double>(-finalDx) / panelWidth;
    wxTimeSpan delta = wxTimeSpan::Seconds(
        m_timelineDuration.GetSeconds().ToLong() * fraction);

    // Update the actual timeline data to exactly match what was visually shown
    m_startTimestamp = m_dragStartTimelineStart + delta;
    m_selectedTimestamp = m_dragStartTimestamp + delta;

    // Reset the drag offset BEFORE the final refresh to avoid double-offset
    m_dragOffsetX = 0;
  }

  m_isDragging = false;
  m_isDraggingIndicator = false;
  m_isDraggingTimeline = false;

  // Reset cursor to default
  m_timelinePanel->SetCursor(wxCursor(wxCURSOR_DEFAULT));

  // Final refresh to show the committed state (no visual offset anymore)
  UpdateTimelineDisplay();
}

void Timeline::OnMouseLeave(wxMouseEvent& event) {
  if (!m_isDragging) {
    // Reset cursor to default when mouse leaves the timeline panel
    m_timelinePanel->SetCursor(wxCursor(wxCURSOR_DEFAULT));
  }
}

void Timeline::UpdateTimelineDisplay() {
  m_timestampLabel->SetLabel(FormatTimestamp(m_selectedTimestamp));
  m_timelinePanel->Refresh();
}

void Timeline::EnsureTimestampVisible() {
  if (m_selectedTimestamp < m_startTimestamp)
    m_startTimestamp = m_selectedTimestamp;
  else if (m_selectedTimestamp > m_startTimestamp + m_timelineDuration)
    m_startTimestamp = m_selectedTimestamp - m_timelineDuration;
}

wxString Timeline::FormatTimestamp(const wxDateTime& timestamp) const {
  return timestamp.Format("%Y-%m-%d %H:%M %Z");
}

std::pair<wxString, int> Timeline::GetTimeUnitAndInterval() const {
  if (m_timelineDuration <= wxTimeSpan::Days(1))
    return std::make_pair("hour", 1);
  else if (m_timelineDuration <= wxTimeSpan::Days(2))
    return std::make_pair("hour", 4);
  else if (m_timelineDuration <= wxTimeSpan::Days(30))
    return std::make_pair("day", 1);
  else if (m_timelineDuration <= wxTimeSpan::Days(60))
    return std::make_pair("day", 7);
  else if (m_timelineDuration <= wxTimeSpan::Days(365))
    return std::make_pair("month", 1);
  else
    return std::make_pair("year", 1);
}

wxString Timeline::FormatDate(const wxDateTime& date, const wxString& timeUnit,
                              int availableWidth) const {
  if (timeUnit == "hour")
    return date.Format("%H:%M");
  else if (timeUnit == "day")
    return date.Format("%d %b");
  else if (timeUnit == "month")
    return date.Format("%b %Y");
  else  // year
    return date.Format("%Y");
}

bool Timeline::IsClickOnTimeIndicator(int mouseX) const {
  int indicatorX = GetTimeIndicatorPosition(m_selectedTimestamp);
  // Allow a 6-pixel tolerance on each side of the indicator line
  return (mouseX >= indicatorX - 6 && mouseX <= indicatorX + 6);
}

int Timeline::GetTimeIndicatorPosition(const wxDateTime& timestamp) const {
  wxSize size = m_timelinePanel->GetSize();
  int width = size.GetWidth();

  double fraction = (timestamp - m_startTimestamp).GetSeconds().ToDouble() /
                    m_timelineDuration.GetSeconds().ToDouble();
  return static_cast<int>(fraction * width);
}
