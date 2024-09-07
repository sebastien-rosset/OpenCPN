/******************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  Transparent Icon Button
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

#include "icon_button.h"
#include <wx/graphics.h>

wxBEGIN_EVENT_TABLE(IconButton, wxPanel)
    EVT_PAINT(IconButton::OnPaint)
    EVT_ENTER_WINDOW(IconButton::OnMouseEnter)
    EVT_LEAVE_WINDOW(IconButton::OnMouseLeave)
    EVT_LEFT_DOWN(IconButton::OnMouseDown)
    EVT_LEFT_UP(IconButton::OnMouseUp)
wxEND_EVENT_TABLE()

IconButton::IconButton(wxWindow* parent, const wxString& label, const wxSize& size)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, size, wxTRANSPARENT_WINDOW)
{
    m_label = label;
    m_font = wxFont(12, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);

    m_normalColor = wxColour(0, 0, 0, 130);    // Semi-transparent black
    m_hoverColor = wxColour(0, 0, 0, 210);     // More opaque black
    m_clickColor = wxColour(0, 0, 0, 255);     // Fully opaque black

    m_normalBg = wxColour(255, 255, 255, 128); // Semi-transparent white
    m_hoverBg = wxColour(240, 240, 240, 180);  // Slightly darker, more opaque
    m_clickBg = wxColour(220, 220, 220, 220);  // Even darker and more opaque

    m_currentColor = m_normalColor;
    m_currentBg = m_normalBg;
}

void IconButton::SetLabel(const wxString& label)
{
    m_label = label;
    Refresh();
}

void IconButton::OnPaint(wxPaintEvent& event)
{
    wxPaintDC dc(this);
    wxGraphicsContext* gc = wxGraphicsContext::Create(dc);

    if (gc)
    {
        // Set up the graphics context
        gc->SetFont(m_font, m_currentColor);

        // Get the size of the button and the text
        wxSize size = GetSize();
        double textWidth, textHeight;
        gc->GetTextExtent(m_label, &textWidth, &textHeight);

        // Calculate the rounded rectangle dimensions
        double rectRadius = wxMin(size.GetWidth(), size.GetHeight()) * 0.2; // 20% of the smaller dimension

        // Draw the rounded rectangle with current background color
        gc->SetBrush(wxBrush(m_currentBg));
        gc->SetPen(*wxTRANSPARENT_PEN);
        gc->DrawRoundedRectangle(0, 0, size.GetWidth(), size.GetHeight(), rectRadius);

        // Draw the text
        double x = (size.GetWidth() - textWidth) / 2;
        double y = (size.GetHeight() - textHeight) / 2;
        gc->DrawText(m_label, x, y);

        delete gc;
    }
}

void IconButton::OnMouseEnter(wxMouseEvent& event)
{
    m_currentColor = m_hoverColor;
    m_currentBg = m_hoverBg;
    Refresh();
}

void IconButton::OnMouseLeave(wxMouseEvent& event)
{
    m_currentColor = m_normalColor;
    m_currentBg = m_normalBg;
    Refresh();
}

void IconButton::OnMouseDown(wxMouseEvent& event)
{
    m_currentColor = m_clickColor;
    m_currentBg = m_clickBg;
    Refresh();
}

void IconButton::OnMouseUp(wxMouseEvent& event)
{
    m_currentColor = m_hoverColor;
    m_currentBg = m_hoverBg;
    Refresh();

    // Create and post a button event
    wxCommandEvent buttonEvent(wxEVT_BUTTON);
    buttonEvent.SetEventObject(this);
    buttonEvent.SetId(GetId());
    GetEventHandler()->ProcessEvent(buttonEvent);
}