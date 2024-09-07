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

#ifndef ICON_BUTTON_H
#define ICON_BUTTON_H

#include <wx/wx.h>

/**
 * @brief A custom wxPanel that represents a transparent icon button.
 *
 * This class implements a button with a transparent background and
 * customizable appearance for different states (normal, hover, click).
 */
class IconButton : public wxPanel
{
public:
    /**
     * @brief Construct a new IconButton object
     *
     * @param parent The parent window
     * @param label The label text of the button
     * @param size The size of the button
     */
    IconButton(wxWindow* parent, const wxString& label,
               const wxSize& size = wxSize(20, 20));

    /**
     * @brief Set the label of the button
     *
     * @param label The new label text
     */
    void SetLabel(const wxString& label);

    /**
     * @brief Get the current label of the button
     *
     * @return wxString The current label text
     */
    wxString GetLabel() const { return m_label; }

private:
    wxString m_label;
    wxFont m_font;
    wxColour m_normalColor;
    wxColour m_hoverColor;
    wxColour m_clickColor;
    wxColour m_normalBg;
    wxColour m_hoverBg;
    wxColour m_clickBg;
    wxColour m_currentColor;
    wxColour m_currentBg;

    void OnPaint(wxPaintEvent& event);
    void OnMouseEnter(wxMouseEvent& event);
    void OnMouseLeave(wxMouseEvent& event);
    void OnMouseDown(wxMouseEvent& event);
    void OnMouseUp(wxMouseEvent& event);

    wxDECLARE_EVENT_TABLE();
};

#endif // ICON_BUTTON_H