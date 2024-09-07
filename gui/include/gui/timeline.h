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

class IconButton;

/**
 * @brief A custom widget for displaying a timeline
 *
 * This class implements a timeline widget that can display various elements
 * over a specified time range. It includes controls for zooming, playing,
 * and navigating through the timeline.
 */
class Timeline : public wxPanel
{
public:
    /**
     * @brief Construct a new Timeline object
     *
     * @param parent The parent window
     * @param start The start date/time for the timeline
     * @param timelineDuration The duration of the timeline
     */
    Timeline(wxWindow* parent,
             const wxDateTime& start = wxDateTime::Now(),
             const wxTimeSpan& timelineDuration = wxTimeSpan::Days(16));

    /**
     * @brief Set the timeline elements to be displayed
     *
     * @param elements Vector of TimelineElement objects to display
     */
    void SetTimelineElements(const std::vector<TimelineElement>& elements);

private:
    // GUI components
    wxStaticText* m_timestampLabel;
    IconButton* m_playButton;
    IconButton* m_nowButton;
    IconButton* m_leftButton;
    IconButton* m_rightButton;
    IconButton* m_zoomInButton;
    IconButton* m_zoomOutButton;
    wxPanel* m_timelinePanel;
    wxBoxSizer* m_ctrlPanel;

    // Timeline properties
    wxTimeSpan m_timelineDuration;
    wxDateTime m_startTimestamp;
    wxDateTime m_selectedTimestamp;
    std::vector<TimelineElement> m_timelineElements;

    // Other member variables
    bool m_isPlaying;
    wxTimer* m_playTimer;
    wxTimer* m_updateTimer;
    wxTimeSpan m_updateInterval;

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
    void UpdateTimelineDisplay();
    wxString FormatTimestamp(const wxDateTime& timestamp) const;
    std::pair<wxString, int> GetTimeUnitAndInterval() const;
    wxString FormatDate(const wxDateTime& date, const wxString& timeUnit, int availableWidth = 0) const;
    void EnsureTimestampVisible();

    // Add other necessary methods and member variables
};

/**
 * @brief Struct representing an element on the timeline
 */
struct TimelineElement
{
    /**
     * @brief Struct representing a part of a timeline element
     */
    struct Part
    {
        wxDateTime start;     ///< Start time of the part
        wxTimeSpan duration;  ///< Duration of the part
        wxColour color;       ///< Color of the part
        wxString label;       ///< Label for the part
    };

    std::vector<Part> parts;  ///< Vector of parts composing the timeline element
};

#endif // TIMELINE_H