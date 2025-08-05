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

#ifndef METEOGRAM_PANEL_H
#define METEOGRAM_PANEL_H

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include <wx/aui/aui.h>
#include <wx/datetime.h>
#include <vector>
#include <memory>
#include <limits>

class ArrayOfGribRecordSets;
class GribRecord;
class GRIBUICtrlBar;

/**
 * Single data point in meteogram time series
 */
struct MeteogramDataPoint {
  wxDateTime timestamp;

  // Atmospheric data
  double temperature;    // Celsius (from Idx_AIR_TEMP)
  double pressure;       // hPa (from Idx_PRESSURE)
  double humidity;       // % (from Idx_HUMID_RE)
  double cloudCover;     // % (from Idx_CLOUD_TOT)
  double precipitation;  // mm/hour (from Idx_PRECIP_TOT)

  // Wind data
  double windSpeed;      // knots (from Idx_WIND_VX/VY)
  double windDirection;  // degrees (from Idx_WIND_VX/VY)
  double windGust;       // knots (from Idx_WIND_GUST)

  // Wave data
  double waveHeight;     // meters (from Idx_HTSIGW)
  double wavePeriod;     // seconds (from Idx_WVPER)
  double waveDirection;  // degrees (from Idx_WVDIR)

  MeteogramDataPoint()
      : temperature(GRIB_NOTDEF),
        pressure(GRIB_NOTDEF),
        humidity(GRIB_NOTDEF),
        cloudCover(GRIB_NOTDEF),
        precipitation(GRIB_NOTDEF),
        windSpeed(GRIB_NOTDEF),
        windDirection(GRIB_NOTDEF),
        windGust(GRIB_NOTDEF),
        waveHeight(GRIB_NOTDEF),
        wavePeriod(GRIB_NOTDEF),
        waveDirection(GRIB_NOTDEF) {}

  bool IsValid() const;
};

/**
 * Location for meteogram data - either point or route
 */
class MeteogramLocation {
public:
  enum Type { kPoint, kRoute };

  // Point location constructor
  MeteogramLocation(double lat, double lon, const wxString& name = "");

  // Route location constructor
  MeteogramLocation(const std::vector<std::pair<double, double>>& routePoints,
                    const wxString& routeName = "");

  Type GetType() const { return m_type; }
  wxString GetName() const { return m_name; }
  wxString GetDisplayName() const;

  // For point locations
  double GetLatitude() const { return m_lat; }
  double GetLongitude() const { return m_lon; }

  // For route locations
  const std::vector<std::pair<double, double>>& GetRoutePoints() const {
    return m_routePoints;
  }
  std::pair<double, double> GetRoutePointAt(
      double fraction) const;  // 0.0 = start, 1.0 = end

private:
  Type m_type;
  wxString m_name;

  // Point data
  double m_lat, m_lon;

  // Route data
  std::vector<std::pair<double, double>> m_routePoints;
};

/**
 * Container for meteogram data with location context
 */
class MeteogramData {
public:
  MeteogramData();

  // Data management
  void Clear();
  bool IsValid() const { return !m_dataPoints.empty(); }

  // Load data from GRIB record sets
  void LoadFromGribRecordSets(ArrayOfGribRecordSets* recordSets,
                              const MeteogramLocation& location);

  // Accessors
  const std::vector<MeteogramDataPoint>& GetDataPoints() const {
    return m_dataPoints;
  }
  const MeteogramLocation& GetLocation() const { return m_location; }
  wxDateTime GetStartTime() const { return m_startTime; }
  wxDateTime GetEndTime() const { return m_endTime; }

  // Data queries
  MeteogramDataPoint GetDataAtTime(const wxDateTime& time) const;
  std::pair<double, double> GetTemperatureRange() const;
  std::pair<double, double> GetPressureRange() const;
  std::pair<double, double> GetWindSpeedRange() const;

private:
  std::vector<MeteogramDataPoint> m_dataPoints;
  MeteogramLocation m_location;
  wxDateTime m_startTime, m_endTime;

  // Data extraction helpers
  void ExtractPointData(ArrayOfGribRecordSets* recordSets, double lat,
                        double lon);
  void ExtractRouteData(
      ArrayOfGribRecordSets* recordSets,
      const std::vector<std::pair<double, double>>& routePoints);
  MeteogramDataPoint ExtractDataPoint(GribRecord** recordArray, double lat,
                                      double lon);
};

/**
 * Meteogram rendering engine
 */
class MeteogramRenderer {
public:
  MeteogramRenderer();

  // Main rendering
  void RenderMeteogram(wxDC& dc, const wxRect& bounds,
                       const MeteogramData& data,
                       const wxDateTime& selectedTime = wxInvalidDateTime);

  // Mouse interaction
  wxDateTime GetTimeAtPosition(const wxPoint& mousePos, const wxRect& bounds,
                               const MeteogramData& data) const;
  wxString GetTooltipAtPosition(const wxPoint& mousePos, const wxRect& bounds,
                                const MeteogramData& data) const;

  // Layer visibility control
  void SetShowTemperature(bool show) { m_showTemperature = show; }
  void SetShowWind(bool show) { m_showWind = show; }
  void SetShowPressure(bool show) { m_showPressure = show; }
  void SetShowPrecipitation(bool show) { m_showPrecipitation = show; }
  void SetShowWaves(bool show) { m_showWaves = show; }

  bool GetShowTemperature() const { return m_showTemperature; }
  bool GetShowWind() const { return m_showWind; }
  bool GetShowPressure() const { return m_showPressure; }
  bool GetShowPrecipitation() const { return m_showPrecipitation; }
  bool GetShowWaves() const { return m_showWaves; }

  // Layout properties
  int GetMinimumHeight() const { return 200; }
  int GetPreferredHeight() const;

private:
  // Layer rendering methods
  void DrawBackground(wxDC& dc, const wxRect& bounds);
  void DrawTimeAxis(wxDC& dc, const wxRect& bounds, const MeteogramData& data);
  void DrawTemperatureLayer(wxDC& dc, const wxRect& bounds,
                            const MeteogramData& data);
  void DrawWindLayer(wxDC& dc, const wxRect& bounds, const MeteogramData& data);
  void DrawPressureLayer(wxDC& dc, const wxRect& bounds,
                         const MeteogramData& data);
  void DrawPrecipitationLayer(wxDC& dc, const wxRect& bounds,
                              const MeteogramData& data);
  void DrawWaveLayer(wxDC& dc, const wxRect& bounds, const MeteogramData& data);
  void DrawSelectedTimeIndicator(wxDC& dc, const wxRect& bounds,
                                 const MeteogramData& data,
                                 const wxDateTime& selectedTime);

  // Layout helpers
  wxRect GetTimeAxisArea(const wxRect& bounds) const;
  wxRect GetTemperatureArea(const wxRect& bounds) const;
  wxRect GetWindArea(const wxRect& bounds) const;
  wxRect GetPressureArea(const wxRect& bounds) const;
  wxRect GetPrecipitationArea(const wxRect& bounds) const;
  wxRect GetWaveArea(const wxRect& bounds) const;

  // Coordinate conversion
  int TimeToPixel(const wxDateTime& time, const wxRect& bounds,
                  const MeteogramData& data) const;
  wxDateTime PixelToTime(int x, const wxRect& bounds,
                         const MeteogramData& data) const;
  double ValueToPixel(double value, double minVal, double maxVal,
                      const wxRect& area, bool invertY = true) const;

  // Styling
  wxColour GetWindArrowColor(double windSpeed) const;
  wxColour GetTemperatureColor(double temperature) const;
  void DrawWindArrow(wxDC& dc, int x, int y, double speed, double direction,
                     int maxSize);

  // Layer visibility flags
  bool m_showTemperature;
  bool m_showWind;
  bool m_showPressure;
  bool m_showPrecipitation;
  bool m_showWaves;

  // Visual styling
  wxColour m_temperatureColor;
  wxColour m_pressureColor;
  wxColour m_precipitationColor;
  wxColour m_windColor;
  wxColour m_waveColor;
  wxColour m_backgroundColor;
  wxColour m_gridColor;

  wxFont m_labelFont;
  wxFont m_valueFont;
};

/**
 * Core meteogram panel - embeddable widget for timeline integration
 */
class MeteogramPanel : public wxPanel {
public:
  MeteogramPanel(wxWindow* parent, wxWindowID id = wxID_ANY);
  virtual ~MeteogramPanel() = default;

  // Location and data management
  void SetLocation(const MeteogramLocation& location);
  void SetGribRecordSets(ArrayOfGribRecordSets* recordSets);
  void RefreshData();

  // Time selection (timeline integration hooks)
  void SetSelectedTime(const wxDateTime& time);
  wxDateTime GetSelectedTime() const { return m_selectedTime; }

  // Renderer access for configuration
  MeteogramRenderer& GetRenderer() { return m_renderer; }
  const MeteogramData& GetData() const { return m_data; }

protected:
  // Event handlers
  void OnPaint(wxPaintEvent& event);
  void OnSize(wxSizeEvent& event);
  void OnMouseMove(wxMouseEvent& event);
  void OnLeftClick(wxMouseEvent& event);
  void OnRightClick(wxMouseEvent& event);
  void OnLeaveWindow(wxMouseEvent& event);
  void OnMouseWheel(wxMouseEvent& event);

  // Context menu handlers
  void OnShowLayerMenu(wxCommandEvent& event);
  void OnToggleLayer(wxCommandEvent& event);
  void OnExportData(wxCommandEvent& event);
  void OnExportImage(wxCommandEvent& event);

private:
  MeteogramRenderer m_renderer;
  MeteogramData m_data;
  ArrayOfGribRecordSets* m_recordSets;
  MeteogramLocation m_location;
  wxDateTime m_selectedTime;

  // Mouse interaction state
  wxPoint m_lastMousePos;
  bool m_mouseInPanel;
  wxString m_currentTooltip;

  // UI helpers
  void ShowContextMenu(const wxPoint& pos);
  void UpdateTooltip();
  void LoadMeteogramData();

  // Place wxDECLARE_EVENT_TABLE at the end as per your discovery
  wxDECLARE_EVENT_TABLE();
};

/**
 * Dockable meteogram window using wxAUI
 */
class MeteogramWindow : public wxPanel {
public:
  MeteogramWindow(wxWindow* parent, wxAuiManager* auiManager,
                  const MeteogramLocation& location);
  virtual ~MeteogramWindow();

  // AUI integration
  void CreateAuiPane();
  void ShowPane(bool show = true);
  void HidePane() { ShowPane(false); }
  bool IsPaneShown() const;
  void TogglePane() { ShowPane(!IsPaneShown()); }

  // Data management
  void SetGribRecordSets(ArrayOfGribRecordSets* recordSets);
  void RefreshData();

  // Access to meteogram panel
  MeteogramPanel* GetMeteogramPanel() { return m_meteogramPanel; }
  const MeteogramLocation& GetLocation() const { return m_location; }

  // Factory methods
  static MeteogramWindow* CreateForPoint(wxWindow* parent,
                                         wxAuiManager* auiManager, double lat,
                                         double lon, const wxString& name = "");
  static MeteogramWindow* CreateForRoute(
      wxWindow* parent, wxAuiManager* auiManager,
      const std::vector<std::pair<double, double>>& routePoints,
      const wxString& routeName = "");

protected:
  void OnClose(wxCloseEvent& event);
  void OnCloseButton(wxCommandEvent& event);  // For button close events
  void OnRefresh(wxCommandEvent& event);
  void OnSettings(wxCommandEvent& event);
  void OnPaneClose(wxAuiManagerEvent& event);

  // Forward meteogram events
  void OnMeteogramTimeSelected(wxCommandEvent& event);
  void OnMeteogramLocationChanged(wxCommandEvent& event);

private:
  wxAuiManager* m_auiManager;
  MeteogramPanel* m_meteogramPanel;
  MeteogramLocation m_location;
  wxString m_paneName;

  // Toolbar and status
  wxPanel* m_headerPanel;
  wxStaticText* m_locationLabel;
  wxButton* m_refreshButton;
  wxButton* m_settingsButton;
  wxButton* m_closeButton;

  void CreateHeader();
  void UpdateLocationLabel();
  wxString GenerateUniquePaneName() const;

  // Place wxDECLARE_EVENT_TABLE at the end
  wxDECLARE_EVENT_TABLE();
};

// Custom events - declare them properly outside of classes
wxDECLARE_EVENT(EVT_METEOGRAM_TIME_SELECTED, wxCommandEvent);
wxDECLARE_EVENT(EVT_METEOGRAM_LOCATION_CHANGED, wxCommandEvent);

// Menu IDs for context menus
enum {
  ID_METEOGRAM_TOGGLE_TEMPERATURE = 15000,
  ID_METEOGRAM_TOGGLE_WIND,
  ID_METEOGRAM_TOGGLE_PRESSURE,
  ID_METEOGRAM_TOGGLE_PRECIPITATION,
  ID_METEOGRAM_TOGGLE_WAVES,
  ID_METEOGRAM_EXPORT_DATA,
  ID_METEOGRAM_EXPORT_IMAGE,
  ID_METEOGRAM_SETTINGS,
  ID_METEOGRAM_REFRESH,
  ID_METEOGRAM_CLOSE
};

#endif  // METEOGRAM_PANEL_H