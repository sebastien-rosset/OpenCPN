/***************************************************************************
 *   Copyright (C) 2025 by OpenCPN development team                        *
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
 ***************************************************************************/

#include "GribRecord.h"
#include "meteogram_panel.h"
#include "grib_pi.h"
#include "GribUIDialog.h"
#include "ocpn_plugin.h"

#include <wx/dcbuffer.h>
#include <wx/menu.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>

//=============================================================================
// MeteogramPanel Implementation
//=============================================================================

wxBEGIN_EVENT_TABLE(MeteogramPanel, wxPanel) EVT_PAINT(MeteogramPanel::OnPaint)
    EVT_SIZE(MeteogramPanel::OnSize) EVT_MOTION(MeteogramPanel::OnMouseMove)
        EVT_LEFT_UP(MeteogramPanel::OnLeftClick)
            EVT_RIGHT_UP(MeteogramPanel::OnRightClick)
                EVT_LEAVE_WINDOW(MeteogramPanel::OnLeaveWindow)
                    EVT_MOUSEWHEEL(MeteogramPanel::OnMouseWheel)

    // Context menu handlers
    EVT_MENU(ID_METEOGRAM_TOGGLE_TEMPERATURE, MeteogramPanel::OnToggleLayer)
        EVT_MENU(ID_METEOGRAM_TOGGLE_WIND, MeteogramPanel::OnToggleLayer)
            EVT_MENU(ID_METEOGRAM_TOGGLE_PRESSURE,
                     MeteogramPanel::OnToggleLayer)
                EVT_MENU(ID_METEOGRAM_TOGGLE_PRECIPITATION,
                         MeteogramPanel::OnToggleLayer)
                    EVT_MENU(ID_METEOGRAM_TOGGLE_WAVES,
                             MeteogramPanel::OnToggleLayer)
                        EVT_MENU(ID_METEOGRAM_EXPORT_DATA,
                                 MeteogramPanel::OnExportData)
                            EVT_MENU(ID_METEOGRAM_EXPORT_IMAGE,
                                     MeteogramPanel::OnExportImage)
                                wxEND_EVENT_TABLE()

                                    MeteogramPanel::MeteogramPanel(
                                        wxWindow* parent, wxWindowID id)
    : wxPanel(parent, id),
      m_recordSets(nullptr),
      m_location(0, 0),
      m_selectedTime(wxInvalidDateTime),
      m_mouseInPanel(false) {
  SetBackgroundColour(*wxWHITE);
  SetMinSize(wxSize(400, m_renderer.GetMinimumHeight()));

  // Enable tooltip
  SetToolTip(wxEmptyString);
}

void MeteogramPanel::SetLocation(const MeteogramLocation& location) {
  if (m_location.GetType() != location.GetType() ||
      (location.GetType() == MeteogramLocation::kPoint &&
       (m_location.GetLatitude() != location.GetLatitude() ||
        m_location.GetLongitude() != location.GetLongitude())) ||
      (location.GetType() == MeteogramLocation::kRoute &&
       m_location.GetRoutePoints() != location.GetRoutePoints())) {
    m_location = location;
    LoadMeteogramData();

    // Notify listeners of location change
    wxCommandEvent event(EVT_METEOGRAM_LOCATION_CHANGED);
    event.SetEventObject(this);
    event.SetString(m_location.GetDisplayName());
    ProcessEvent(event);
  }
}

void MeteogramPanel::SetGribRecordSets(ArrayOfGribRecordSets* recordSets) {
  m_recordSets = recordSets;
  LoadMeteogramData();
}

void MeteogramPanel::RefreshData() { LoadMeteogramData(); }

void MeteogramPanel::SetSelectedTime(const wxDateTime& time) {
  if (m_selectedTime != time) {
    m_selectedTime = time;
    Refresh();

    // Notify listeners of time selection
    wxCommandEvent event(EVT_METEOGRAM_TIME_SELECTED);
    event.SetEventObject(this);
    event.SetString(time.IsValid() ? time.Format(wxT("%Y-%m-%d %H:%M:%S"))
                                   : wxString(wxEmptyString));
    ProcessEvent(event);
  }
}

void MeteogramPanel::OnPaint(wxPaintEvent& event) {
  wxAutoBufferedPaintDC dc(this);
  wxRect clientRect = GetClientRect();

  m_renderer.RenderMeteogram(dc, clientRect, m_data, m_selectedTime);
}

void MeteogramPanel::OnSize(wxSizeEvent& event) {
  Refresh();
  event.Skip();
}

void MeteogramPanel::OnMouseMove(wxMouseEvent& event) {
  m_lastMousePos = event.GetPosition();
  m_mouseInPanel = true;

  UpdateTooltip();
  event.Skip();
}

void MeteogramPanel::OnLeftClick(wxMouseEvent& event) {
  if (!m_data.IsValid()) return;

  wxRect clientRect = GetClientRect();
  wxDateTime clickTime =
      m_renderer.GetTimeAtPosition(event.GetPosition(), clientRect, m_data);

  if (clickTime.IsValid()) {
    SetSelectedTime(clickTime);
  }
}

void MeteogramPanel::OnRightClick(wxMouseEvent& event) {
  ShowContextMenu(event.GetPosition());
}

void MeteogramPanel::OnLeaveWindow(wxMouseEvent& event) {
  m_mouseInPanel = false;
  SetToolTip(wxEmptyString);
  event.Skip();
}

void MeteogramPanel::OnMouseWheel(wxMouseEvent& event) {
  // Future: could implement zoom functionality here
  event.Skip();
}

void MeteogramPanel::ShowContextMenu(const wxPoint& pos) {
  wxMenu contextMenu;

  // Layer visibility submenu
  wxMenu* layersMenu = new wxMenu();
  layersMenu->AppendCheckItem(ID_METEOGRAM_TOGGLE_TEMPERATURE, _("Temperature"))
      ->Check(m_renderer.GetShowTemperature());
  layersMenu->AppendCheckItem(ID_METEOGRAM_TOGGLE_WIND, _("Wind"))
      ->Check(m_renderer.GetShowWind());
  layersMenu->AppendCheckItem(ID_METEOGRAM_TOGGLE_PRESSURE, _("Pressure"))
      ->Check(m_renderer.GetShowPressure());
  layersMenu
      ->AppendCheckItem(ID_METEOGRAM_TOGGLE_PRECIPITATION, _("Precipitation"))
      ->Check(m_renderer.GetShowPrecipitation());
  layersMenu->AppendCheckItem(ID_METEOGRAM_TOGGLE_WAVES, _("Waves"))
      ->Check(m_renderer.GetShowWaves());

  contextMenu.AppendSubMenu(layersMenu, _("Show Layers"));
  contextMenu.AppendSeparator();
  contextMenu.Append(ID_METEOGRAM_EXPORT_DATA, _("Export Data..."));
  contextMenu.Append(ID_METEOGRAM_EXPORT_IMAGE, _("Export Image..."));

  PopupMenu(&contextMenu, pos);
}

void MeteogramPanel::OnToggleLayer(wxCommandEvent& event) {
  switch (event.GetId()) {
    case ID_METEOGRAM_TOGGLE_TEMPERATURE:
      m_renderer.SetShowTemperature(!m_renderer.GetShowTemperature());
      break;
    case ID_METEOGRAM_TOGGLE_WIND:
      m_renderer.SetShowWind(!m_renderer.GetShowWind());
      break;
    case ID_METEOGRAM_TOGGLE_PRESSURE:
      m_renderer.SetShowPressure(!m_renderer.GetShowPressure());
      break;
    case ID_METEOGRAM_TOGGLE_PRECIPITATION:
      m_renderer.SetShowPrecipitation(!m_renderer.GetShowPrecipitation());
      break;
    case ID_METEOGRAM_TOGGLE_WAVES:
      m_renderer.SetShowWaves(!m_renderer.GetShowWaves());
      break;
  }

  // Update preferred size based on visible layers
  SetMinSize(wxSize(400, m_renderer.GetPreferredHeight()));

  Refresh();
  GetParent()->Layout();  // Update parent layout
}

void MeteogramPanel::OnExportData(wxCommandEvent& event) {
  if (!m_data.IsValid()) {
    wxMessageBox(_("No data to export"), _("Export Error"),
                 wxOK | wxICON_WARNING);
    return;
  }

  wxFileDialog saveDialog(this, _("Export Meteogram Data"), "",
                          m_location.GetName() + "_meteogram.csv",
                          "CSV files (*.csv)|*.csv",
                          wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

  if (saveDialog.ShowModal() == wxID_OK) {
    wxString filename = saveDialog.GetPath();

    wxFile file(filename, wxFile::write);
    if (!file.IsOpened()) {
      wxMessageBox(_("Could not open file for writing"), _("Export Error"),
                   wxOK | wxICON_ERROR);
      return;
    }

    // Write CSV header
    wxString tempUnit = getUsrTempUnit_Plugin();
    wxString windUnit = getUsrWindSpeedUnit_Plugin();
    file.Write(wxString::Format(
        "Timestamp,Temperature(%s),Wind Speed(%s),Wind Direction(deg),"
        "Pressure(hPa),Precipitation(mm/h),Humidity(%),Cloud Cover(%),"
        "Wave Height(m),Wave Period(s),Wave Direction(deg)\n",
        tempUnit, windUnit));

    // Write data points
    for (const auto& point : m_data.GetDataPoints()) {
      wxString line = point.timestamp.Format("%Y-%m-%d %H:%M:%S") + ",";
      line +=
          (point.temperature != GRIB_NOTDEF)
              ? wxString::Format("%.2f", toUsrTemp_Plugin(point.temperature))
              : "";
      line += ",";
      line +=
          (point.windSpeed != GRIB_NOTDEF)
              ? wxString::Format("%.2f", toUsrWindSpeed_Plugin(point.windSpeed))
              : "";
      line += ",";
      line += (point.windDirection != GRIB_NOTDEF)
                  ? wxString::Format("%.1f", point.windDirection)
                  : "";
      line += ",";
      line += (point.pressure != GRIB_NOTDEF)
                  ? wxString::Format("%.1f", point.pressure)
                  : "";
      line += ",";
      line += (point.precipitation != GRIB_NOTDEF)
                  ? wxString::Format("%.2f", point.precipitation)
                  : "";
      line += ",";
      line += (point.humidity != GRIB_NOTDEF)
                  ? wxString::Format("%.1f", point.humidity)
                  : "";
      line += ",";
      line += (point.cloudCover != GRIB_NOTDEF)
                  ? wxString::Format("%.1f", point.cloudCover)
                  : "";
      line += ",";
      line += (point.waveHeight != GRIB_NOTDEF)
                  ? wxString::Format("%.2f", point.waveHeight)
                  : "";
      line += ",";
      line += (point.wavePeriod != GRIB_NOTDEF)
                  ? wxString::Format("%.1f", point.wavePeriod)
                  : "";
      line += ",";
      line += (point.waveDirection != GRIB_NOTDEF)
                  ? wxString::Format("%.1f", point.waveDirection)
                  : "";
      line += "\n";

      file.Write(line);
    }

    file.Close();
    wxMessageBox(_("Data exported successfully"), _("Export Complete"),
                 wxOK | wxICON_INFORMATION);
  }
}

void MeteogramPanel::OnExportImage(wxCommandEvent& event) {
  wxFileDialog saveDialog(this, _("Export Meteogram Image"), "",
                          m_location.GetName() + "_meteogram.png",
                          "PNG files (*.png)|*.png",
                          wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

  if (saveDialog.ShowModal() == wxID_OK) {
    wxString filename = saveDialog.GetPath();

    // Create bitmap of current meteogram
    wxSize size = GetSize();
    wxBitmap bitmap(size.x, size.y);
    wxMemoryDC memDC(bitmap);

    m_renderer.RenderMeteogram(memDC, wxRect(0, 0, size.x, size.y), m_data,
                               m_selectedTime);
    memDC.SelectObject(wxNullBitmap);

    if (bitmap.SaveFile(filename, wxBITMAP_TYPE_PNG)) {
      wxMessageBox(_("Image exported successfully"), _("Export Complete"),
                   wxOK | wxICON_INFORMATION);
    } else {
      wxMessageBox(_("Could not save image file"), _("Export Error"),
                   wxOK | wxICON_ERROR);
    }
  }
}

void MeteogramPanel::UpdateTooltip() {
  if (!m_mouseInPanel || !m_data.IsValid()) {
    m_currentTooltip = wxEmptyString;
    SetToolTip(wxEmptyString);
    return;
  }

  wxRect clientRect = GetClientRect();
  wxString newTooltip =
      m_renderer.GetTooltipAtPosition(m_lastMousePos, clientRect, m_data);

  if (newTooltip != m_currentTooltip) {
    m_currentTooltip = newTooltip;
    SetToolTip(newTooltip);
  }
}

void MeteogramPanel::LoadMeteogramData() {
  if (m_recordSets) {
    m_data.LoadFromGribRecordSets(m_recordSets, m_location);

    // Update preferred size based on visible layers and data availability
    SetMinSize(wxSize(400, m_renderer.GetPreferredHeight()));

    Refresh();
    if (GetParent()) {
      GetParent()->Layout();
    }
  } else {
    m_data.Clear();
    Refresh();
  }
}

//=============================================================================
// MeteogramWindow Implementation
//=============================================================================

wxBEGIN_EVENT_TABLE(MeteogramWindow, wxPanel)
    EVT_CLOSE(MeteogramWindow::OnClose) EVT_BUTTON(ID_METEOGRAM_REFRESH,
                                                   MeteogramWindow::OnRefresh)
        EVT_BUTTON(ID_METEOGRAM_SETTINGS, MeteogramWindow::OnSettings)
            EVT_BUTTON(ID_METEOGRAM_CLOSE, MeteogramWindow::OnCloseButton)
                EVT_COMMAND(wxID_ANY, EVT_METEOGRAM_TIME_SELECTED,
                            MeteogramWindow::OnMeteogramTimeSelected)
                    EVT_COMMAND(wxID_ANY, EVT_METEOGRAM_LOCATION_CHANGED,
                                MeteogramWindow::OnMeteogramLocationChanged)
                        EVT_AUI_PANE_CLOSE(MeteogramWindow::OnPaneClose)
                            wxEND_EVENT_TABLE()

                                MeteogramWindow::MeteogramWindow(
                                    wxWindow* parent, wxAuiManager* auiManager,
                                    const MeteogramLocation& location)
    : wxPanel(parent, wxID_ANY),
      m_auiManager(auiManager),
      m_location(location),
      m_paneName(GenerateUniquePaneName()) {
  SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));

  // Create main layout
  wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

  // Create header panel
  CreateHeader();
  mainSizer->Add(m_headerPanel, 0, wxEXPAND);

  // Create meteogram panel
  m_meteogramPanel = new MeteogramPanel(this);
  m_meteogramPanel->SetLocation(m_location);
  mainSizer->Add(m_meteogramPanel, 1, wxEXPAND);

  SetSizer(mainSizer);

  // Forward meteogram events
  m_meteogramPanel->Bind(EVT_METEOGRAM_TIME_SELECTED,
                         &MeteogramWindow::OnMeteogramTimeSelected, this);
  m_meteogramPanel->Bind(EVT_METEOGRAM_LOCATION_CHANGED,
                         &MeteogramWindow::OnMeteogramLocationChanged, this);

  UpdateLocationLabel();
}

MeteogramWindow::~MeteogramWindow() {
  if (m_auiManager && IsPaneShown()) {
    HidePane();
  }
}

void MeteogramWindow::CreateHeader() {
  m_headerPanel = new wxPanel(this, wxID_ANY);
  m_headerPanel->SetBackgroundColour(
      wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));

  wxBoxSizer* headerSizer = new wxBoxSizer(wxHORIZONTAL);

  // Location label
  m_locationLabel = new wxStaticText(m_headerPanel, wxID_ANY, "");
  wxFont labelFont = m_locationLabel->GetFont();
  labelFont.SetWeight(wxFONTWEIGHT_BOLD);
  m_locationLabel->SetFont(labelFont);
  headerSizer->Add(m_locationLabel, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);

  // Control buttons
  m_refreshButton =
      new wxButton(m_headerPanel, ID_METEOGRAM_REFRESH, _("Refresh"),
                   wxDefaultPosition, wxSize(60, 24));
  m_settingsButton =
      new wxButton(m_headerPanel, ID_METEOGRAM_SETTINGS, _("Settings"),
                   wxDefaultPosition, wxSize(60, 24));
  m_closeButton = new wxButton(m_headerPanel, ID_METEOGRAM_CLOSE, _("×"),
                               wxDefaultPosition, wxSize(24, 24));

  m_refreshButton->SetToolTip(_("Refresh meteogram data"));
  m_settingsButton->SetToolTip(_("Meteogram display settings"));
  m_closeButton->SetToolTip(_("Close meteogram"));

  headerSizer->Add(m_refreshButton, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
  headerSizer->Add(m_settingsButton, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
  headerSizer->Add(m_closeButton, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);

  m_headerPanel->SetSizer(headerSizer);
}

void MeteogramWindow::CreateAuiPane() {
  if (!m_auiManager) return;

  wxAuiPaneInfo paneInfo;
  paneInfo.Name(m_paneName)
      .Caption(wxString::Format(_("Meteogram - %s"), m_location.GetName()))
      .Dockable(true)
      .Floatable(true)
      .Resizable(true)
      .CloseButton(true)
      .MaximizeButton(false)
      .MinimizeButton(false)
      .PinButton(true)
      .DefaultPane()
      .Right()
      .MinSize(400, m_meteogramPanel->GetRenderer().GetMinimumHeight() + 60)
      .BestSize(600, m_meteogramPanel->GetRenderer().GetPreferredHeight() + 60)
      .FloatingSize(700,
                    m_meteogramPanel->GetRenderer().GetPreferredHeight() + 80);

  m_auiManager->AddPane(this, paneInfo);
  m_auiManager->Update();
}

void MeteogramWindow::ShowPane(bool show) {
  if (!m_auiManager) return;

  wxAuiPaneInfo& pane = m_auiManager->GetPane(m_paneName);
  if (pane.IsOk()) {
    pane.Show(show);
    m_auiManager->Update();
  }
}

bool MeteogramWindow::IsPaneShown() const {
  if (!m_auiManager) return false;

  wxAuiPaneInfo& pane = m_auiManager->GetPane(m_paneName);
  return pane.IsOk() && pane.IsShown();
}

void MeteogramWindow::SetGribRecordSets(ArrayOfGribRecordSets* recordSets) {
  m_meteogramPanel->SetGribRecordSets(recordSets);
}

void MeteogramWindow::RefreshData() { m_meteogramPanel->RefreshData(); }

void MeteogramWindow::UpdateLocationLabel() {
  if (m_locationLabel) {
    m_locationLabel->SetLabel(m_location.GetDisplayName());
    m_headerPanel->Layout();
  }
}

wxString MeteogramWindow::GenerateUniquePaneName() const {
  return wxString::Format("meteogram_%p_%ld", this, wxGetLocalTime());
}

// Factory methods
MeteogramWindow* MeteogramWindow::CreateForPoint(wxWindow* parent,
                                                 wxAuiManager* auiManager,
                                                 double lat, double lon,
                                                 const wxString& name) {
  MeteogramLocation location(lat, lon, name);
  auto* window = new MeteogramWindow(parent, auiManager, location);
  window->CreateAuiPane();
  return window;
}

MeteogramWindow* MeteogramWindow::CreateForRoute(
    wxWindow* parent, wxAuiManager* auiManager,
    const std::vector<std::pair<double, double>>& routePoints,
    const wxString& routeName) {
  MeteogramLocation location(routePoints, routeName);
  auto* window = new MeteogramWindow(parent, auiManager, location);
  window->CreateAuiPane();
  return window;
}

// Event handlers
void MeteogramWindow::OnClose(wxCloseEvent& event) { HidePane(); }

void MeteogramWindow::OnCloseButton(wxCommandEvent& event) { HidePane(); }

void MeteogramWindow::OnRefresh(wxCommandEvent& event) { RefreshData(); }

void MeteogramWindow::OnSettings(wxCommandEvent& event) {
  // Future: Open meteogram settings dialog
  wxMessageBox(_("Meteogram settings not yet implemented"), _("Settings"),
               wxOK | wxICON_INFORMATION);
}

void MeteogramWindow::OnPaneClose(wxAuiManagerEvent& event) {
  if (event.GetPane()->name == m_paneName) {
    // Pane is being closed - clean up
    event.Skip();
  }
}

void MeteogramWindow::OnMeteogramTimeSelected(wxCommandEvent& event) {
  // Forward the event to parent (future: timeline integration)
  event.SetEventObject(this);
  GetParent()->GetEventHandler()->ProcessEvent(event);
}

void MeteogramWindow::OnMeteogramLocationChanged(wxCommandEvent& event) {
  UpdateLocationLabel();

  // Update pane caption
  if (m_auiManager) {
    wxAuiPaneInfo& pane = m_auiManager->GetPane(m_paneName);
    if (pane.IsOk()) {
      pane.Caption(wxString::Format(_("Meteogram - %s"), m_location.GetName()));
      m_auiManager->Update();
    }
  }
}
