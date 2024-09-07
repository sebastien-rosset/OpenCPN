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

#include "timeline.h"
#include "icon_button.h"
#include <wx/graphics.h>
#include <wx/dcbuffer.h>

Timeline::Timeline(wxWindow* parent, const wxDateTime& start, const wxTimeSpan& timelineDuration)
    : wxPanel(parent), m_timelineDuration(timelineDuration),
      m_startTimestamp(start), m_selectedTimestamp(start),
      m_isDragging(false), m_dragStartX(0)
{
    m_isPlaying = false;
    m_playTimer = new wxTimer(this);
    m_updateTimer = new wxTimer(this);
    m_updateInterval = wxTimeSpan::Milliseconds(250);

    // Create GUI components
    m_timestampLabel = new wxStaticText(this, wxID_ANY, FormatTimestamp(m_selectedTimestamp));
    wxFont font = m_timestampLabel->GetFont();
    font.SetPointSize(10);
    font.SetWeight(wxFONTWEIGHT_BOLD);
    m_timestampLabel->SetFont(font);

    m_playButton = new IconButton(this, "►");
    m_nowButton = new IconButton(this, "⦿");
    m_leftButton = new IconButton(this, "❮");
    m_rightButton = new IconButton(this, "❯");
    m_zoomInButton = new IconButton(this, "+");
    m_zoomOutButton = new IconButton(this, "-");

    m_timelinePanel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTRANSPARENT_WINDOW);
    m_timelinePanel->SetMinSize(wxSize(-1, 45));
    m_timelinePanel->SetMaxSize(wxSize(-1, 45));

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

    // Layout
    m_ctrlPanel = new wxBoxSizer(wxHORIZONTAL);
    m_ctrlPanel->Add(m_playButton, 0, wxALIGN_CENTER_VERTICAL);
    m_ctrlPanel->Add(m_timelinePanel, 1, wxEXPAND);
    m_ctrlPanel->Add(m_zoomOutButton, 0, wxALIGN_CENTER_VERTICAL);
    m_ctrlPanel->Add(m_zoomInButton, 0, wxALIGN_CENTER_VERTICAL);
    m_ctrlPanel->Add(m_nowButton, 0, wxALIGN_CENTER_VERTICAL);
    m_ctrlPanel->Add(m_leftButton, 0, wxALIGN_CENTER_VERTICAL);
    m_ctrlPanel->Add(m_rightButton, 0, wxALIGN_CENTER_VERTICAL);

    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
    mainSizer->Add(m_timestampLabel, 0, wxALIGN_CENTER_HORIZONTAL | wxTOP, 5);
    mainSizer->Add(m_ctrlPanel, 0, wxEXPAND | wxALL, 5);

    SetSizer(mainSizer);

    m_updateTimer->Start(60000); // Update every minute
}

Timeline::~Timeline()
{
    delete m_playTimer;
    delete m_updateTimer;
}

void Timeline::SetTimelineElements(const std::vector<TimelineElement>& elements)
{
    m_timelineElements = elements;
    m_timelinePanel->Refresh();
}

void Timeline::OnPaint(wxPaintEvent& event)
{
    wxAutoBufferedPaintDC dc(m_timelinePanel);
    dc.Clear();
    DrawTimeline(dc);
}

void Timeline::OnSize(wxSizeEvent& event)
{
    m_timelinePanel->Refresh();
    event.Skip();
}

void Timeline::DrawTimeline(wxDC& dc)
{
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

    // Draw the time indicator for the selected time
    DrawTimeIndicator(dc, m_selectedTimestamp);

    // Draw "now" indicator if it's within the visible range
    wxDateTime now = wxDateTime::Now();
    if (m_startTimestamp <= now && now <= m_startTimestamp + m_timelineDuration)
    {
        DrawTimeIndicator(dc, now, wxColour(0, 0, 200)); // Blue for "now"
    }
}

void Timeline::DrawTimeUnits(wxDC& dc, const wxString& timeUnit, int interval)
{
    wxSize size = m_timelinePanel->GetSize();
    int width = size.GetWidth();
    int height = size.GetHeight();

    wxDateTime current = m_startTimestamp;
    while (current <= m_startTimestamp + m_timelineDuration)
    {
        double fraction = (current - m_startTimestamp).GetSeconds().ToDouble() /
                          m_timelineDuration.GetSeconds().ToDouble();
        int x = static_cast<int>(fraction * width);

        wxString label = FormatDate(current, timeUnit);
        wxSize textSize = dc.GetTextExtent(label);

        dc.SetTextForeground(wxColour(100, 100, 100));
        dc.DrawText(label, x - textSize.GetWidth() / 2, 0);

        if (timeUnit == "hour")
            current += wxTimeSpan::Hours(interval);
        else if (timeUnit == "day")
            current += wxDateSpan::Days(interval);
        else if (timeUnit == "month")
            current += wxDateSpan::Months(interval);
        else if (timeUnit == "year")
            current += wxDateSpan::Years(interval);
    }
}

void Timeline::DrawTimelineElements(wxDC& dc)
{
    wxSize size = m_timelinePanel->GetSize();
    int width = size.GetWidth();
    int height = size.GetHeight();

    int elementHeight = 8;
    int elementPadding = 2;
    int startY = 20; // Start below the time unit labels

    for (size_t i = 0; i < m_timelineElements.size(); ++i)
    {
        int elementY = startY + i * (elementHeight + elementPadding);

        for (const auto& part : m_timelineElements[i].parts)
        {
            double startFraction = (part.start - m_startTimestamp).GetSeconds().ToDouble() /
                                   m_timelineDuration.GetSeconds().ToDouble();
            double endFraction = (part.start + part.duration - m_startTimestamp).GetSeconds().ToDouble() /
                                 m_timelineDuration.GetSeconds().ToDouble();

            int startX = static_cast<int>(startFraction * width);
            int endX = static_cast<int>(endFraction * width);

            dc.SetPen(wxPen(part.color, elementHeight));
            dc.DrawLine(startX, elementY + elementHeight / 2, endX, elementY + elementHeight / 2);
        }
    }
}

void Timeline::DrawTimeIndicator(wxDC& dc, const wxDateTime& timestamp, const wxColour& color)
{
    wxSize size = m_timelinePanel->GetSize();
    int width = size.GetWidth();
    int height = size.GetHeight();

    double fraction = (timestamp - m_startTimestamp).GetSeconds().ToDouble() /
                      m_timelineDuration.GetSeconds().ToDouble();
    int x = static_cast<int>(fraction * width);

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
}

void Timeline::OnPlayPause(wxCommandEvent& event)
{
    m_isPlaying = !m_isPlaying;
    if (m_isPlaying)
    {
        m_playButton->SetLabel("⏸");
        m_playTimer->Start(m_updateInterval.GetMilliseconds().GetLo());
    }
    else
    {
        m_playButton->SetLabel("►");
        m_playTimer->Stop();
    }
}

void Timeline::OnPlayTimer(wxTimerEvent& event)
{
    wxTimeSpan increment = CalculateTimeIncrement();
    m_selectedTimestamp += increment;
    m_timestampLabel->SetLabel(FormatTimestamp(m_selectedTimestamp));
    m_timelinePanel->Refresh();

    if (m_selectedTimestamp >= m_startTimestamp + m_timelineDuration)
    {
        m_playTimer->Stop();
        m_isPlaying = false;
        m_playButton->SetLabel("►");
    }
}

wxTimeSpan Timeline::CalculateTimeIncrement() const
{
    wxTimeSpan targetTraversalTime = wxTimeSpan::Seconds(60);
    double traversalTicks = targetTraversalTime.GetMilliseconds().ToDouble() /
                            m_updateInterval.GetMilliseconds().ToDouble();
    wxLongLong milliseconds = m_timelineDuration.GetMilliseconds() / static_cast<long long>(traversalTicks);
    return wxTimeSpan::Milliseconds(milliseconds);
}

void Timeline::OnZoomIn(wxCommandEvent& event) {
    if (m_timelineDuration > wxTimeSpan::Hours(4)) {
        m_timelineDuration = wxTimeSpan::Milliseconds(m_timelineDuration.GetMilliseconds().GetValue() / 2);
        UpdateTimelineDisplay();
    }
}

void Timeline::OnZoomOut(wxCommandEvent& event) {
    if (m_timelineDuration < wxTimeSpan::Days(365 * 4)) {
        m_timelineDuration = wxTimeSpan::Milliseconds(m_timelineDuration.GetMilliseconds().GetValue() * 2);
        UpdateTimelineDisplay();
    }
}


void Timeline::OnNowButton(wxCommandEvent& event)
{
    m_selectedTimestamp = wxDateTime::Now();
    EnsureTimestampVisible();
    UpdateTimelineDisplay();
}

void Timeline::OnMoveBackward(wxCommandEvent& event)
{
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
    if (m_isPlaying)
        OnPlayPause(wxCommandEvent());

    m_isDragging = true;
    m_dragStartX = event.GetX();
    m_dragStartTimestamp = m_selectedTimestamp;
}

void Timeline::OnMouseMove(wxMouseEvent& event) {
    if (m_isDragging) {
        int dx = event.GetX() - m_dragStartX;
        double fraction = static_cast<double>(dx) / m_timelinePanel->GetSize().GetWidth();
        wxTimeSpan delta = wxTimeSpan::Seconds(m_timelineDuration.GetSeconds().ToLong() * fraction);
        m_selectedTimestamp = m_dragStartTimestamp + delta;
        EnsureTimestampVisible();
        UpdateTimelineDisplay();
    }
}

void Timeline::OnMouseUp(wxMouseEvent& event) {
    m_isDragging = false;
}

void Timeline::UpdateTimelineDisplay()
{
    m_timestampLabel->SetLabel(FormatTimestamp(m_selectedTimestamp));
    m_timelinePanel->Refresh();
}

void Timeline::EnsureTimestampVisible()
{
    if (m_selectedTimestamp < m_startTimestamp)
        m_startTimestamp = m_selectedTimestamp;
    else if (m_selectedTimestamp > m_startTimestamp + m_timelineDuration)
        m_startTimestamp = m_selectedTimestamp - m_timelineDuration;
}

wxString Timeline::FormatTimestamp(const wxDateTime& timestamp) const
{
    return timestamp.Format("%Y-%m-%d %H:%M %Z");
}

std::pair<wxString, int> Timeline::GetTimeUnitAndInterval() const
{
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

wxString Timeline::FormatDate(const wxDateTime& date, const wxString& timeUnit) const
{
    if (timeUnit == "hour")
        return date.Format("%H:%M");
    else if (timeUnit == "day")
        return date.Format("%d %b");
    else if (timeUnit == "month")
        return date.Format("%b %Y");
    else // year
        return date.Format("%Y");
}