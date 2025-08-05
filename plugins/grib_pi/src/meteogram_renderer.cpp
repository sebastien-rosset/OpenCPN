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

#include <cmath>

#include <wx/dcbuffer.h>
#include <wx/graphics.h>

#include "GribRecord.h"
#include "meteogram_panel.h"
#include "ocpn_plugin.h"

//=============================================================================
// MeteogramRenderer Implementation
//=============================================================================

MeteogramRenderer::MeteogramRenderer()
    : m_showTemperature(true),
      m_showWind(true),
      m_showPressure(true),
      m_showPrecipitation(true),
      m_showWaves(false),
      m_temperatureColor(220, 50, 50)  // Red
      ,
      m_pressureColor(50, 150, 50)  // Green
      ,
      m_precipitationColor(100, 100, 200)  // Light blue
      ,
      m_windColor(50, 120, 200)  // Blue
      ,
      m_waveColor(0, 150, 150)  // Teal
      ,
      m_backgroundColor(255, 255, 255)  // White
      ,
      m_gridColor(200, 200, 200)  // Light gray
{
  m_labelFont =
      wxFont(9, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
  m_valueFont =
      wxFont(8, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
}

int MeteogramRenderer::GetPreferredHeight() const {
  int height = 60;  // Base height for time axis

  if (m_showTemperature) height += 80;
  if (m_showWind) height += 60;
  if (m_showPressure) height += 80;
  if (m_showPrecipitation) height += 40;
  if (m_showWaves) height += 60;

  return std::max(height, GetMinimumHeight());
}

void MeteogramRenderer::RenderMeteogram(wxDC& dc, const wxRect& bounds,
                                        const MeteogramData& data,
                                        const wxDateTime& selectedTime) {
  if (!data.IsValid()) {
    dc.SetTextForeground(*wxBLACK);
    dc.SetFont(m_labelFont);
    dc.DrawText(_("No GRIB data available"), bounds.x + 10, bounds.y + 10);
    return;
  }

  // Draw background
  DrawBackground(dc, bounds);

  // Draw layers from background to foreground
  if (m_showPrecipitation) DrawPrecipitationLayer(dc, bounds, data);
  if (m_showPressure) DrawPressureLayer(dc, bounds, data);
  if (m_showTemperature) DrawTemperatureLayer(dc, bounds, data);
  if (m_showWaves) DrawWaveLayer(dc, bounds, data);
  if (m_showWind) DrawWindLayer(dc, bounds, data);

  // Draw time axis on top
  DrawTimeAxis(dc, bounds, data);

  // Draw selected time indicator
  if (selectedTime.IsValid()) {
    DrawSelectedTimeIndicator(dc, bounds, data, selectedTime);
  }
}

void MeteogramRenderer::DrawBackground(wxDC& dc, const wxRect& bounds) {
  dc.SetBrush(wxBrush(m_backgroundColor));
  dc.SetPen(wxPen(m_backgroundColor));
  dc.DrawRectangle(bounds);
}

void MeteogramRenderer::DrawTimeAxis(wxDC& dc, const wxRect& bounds,
                                     const MeteogramData& data) {
  if (data.GetDataPoints().empty()) return;

  wxRect timeArea = GetTimeAxisArea(bounds);

  dc.SetFont(m_labelFont);
  dc.SetTextForeground(*wxBLACK);
  dc.SetPen(wxPen(m_gridColor, 1));

  const auto& points = data.GetDataPoints();

  // Draw vertical grid lines and time labels
  for (size_t i = 0; i < points.size(); i++) {
    const auto& point = points[i];
    int x = TimeToPixel(point.timestamp, bounds, data);

    // Draw grid line
    dc.DrawLine(x, bounds.y, x, bounds.y + bounds.height);

    // Draw time label (every few hours to avoid crowding)
    if (i % 3 == 0 || i == points.size() - 1) {
      wxString timeStr;
      if (points.size() > 24) {
        // Long forecast - show date
        timeStr = point.timestamp.Format("%m/%d\n%H:%M");
      } else {
        // Short forecast - show time
        timeStr = point.timestamp.Format("%H:%M");
      }

      wxSize textSize = dc.GetTextExtent(timeStr);
      dc.DrawText(timeStr, x - textSize.x / 2, timeArea.y);
    }
  }
}

void MeteogramRenderer::DrawTemperatureLayer(wxDC& dc, const wxRect& bounds,
                                             const MeteogramData& data) {
  if (!m_showTemperature) return;

  wxRect tempArea = GetTemperatureArea(bounds);
  auto [minTemp, maxTemp] = data.GetTemperatureRange();

  const auto& points = data.GetDataPoints();
  std::vector<wxPoint> tempPoints;

  // Collect temperature curve points
  for (const auto& point : points) {
    if (point.temperature != GRIB_NOTDEF) {
      int x = TimeToPixel(point.timestamp, bounds, data);
      int y = ValueToPixel(point.temperature, minTemp, maxTemp, tempArea);
      tempPoints.push_back(wxPoint(x, y));
    }
  }

  if (tempPoints.size() > 1) {
    // Draw temperature curve
    dc.SetPen(wxPen(m_temperatureColor, 2));
    dc.DrawLines(tempPoints.size(), tempPoints.data());

    // Draw temperature values at key points
    dc.SetFont(m_valueFont);
    dc.SetTextForeground(m_temperatureColor);

    for (size_t i = 0; i < tempPoints.size(); i += 4) {  // Every 4th point
      if (i < points.size() && points[i].temperature != GRIB_NOTDEF) {
        double userTemp = toUsrTemp_Plugin(points[i].temperature);
        wxString tempUnit = getUsrTempUnit_Plugin();
        wxString tempStr = wxString::Format("%.1f%s", userTemp, tempUnit);
        wxSize textSize = dc.GetTextExtent(tempStr);
        dc.DrawText(tempStr, tempPoints[i].x - textSize.x / 2,
                    tempPoints[i].y - textSize.y - 2);
      }
    }
  }

  // Draw temperature axis labels
  dc.SetFont(m_labelFont);
  dc.SetTextForeground(m_temperatureColor);
  double userMinTemp = toUsrTemp_Plugin(minTemp);
  double userMaxTemp = toUsrTemp_Plugin(maxTemp);
  wxString tempUnit = getUsrTempUnit_Plugin();
  wxString tempLabel = wxString::Format("Temperature (%.0f - %.0f%s)",
                                        userMinTemp, userMaxTemp, tempUnit);
  dc.DrawText(tempLabel, tempArea.x, tempArea.y - 20);
}

void MeteogramRenderer::DrawWindLayer(wxDC& dc, const wxRect& bounds,
                                      const MeteogramData& data) {
  if (!m_showWind) return;

  wxRect windArea = GetWindArea(bounds);
  const auto& points = data.GetDataPoints();

  dc.SetFont(m_valueFont);

  // Draw wind arrows
  for (const auto& point : points) {
    if (point.windSpeed != GRIB_NOTDEF && point.windDirection != GRIB_NOTDEF) {
      int x = TimeToPixel(point.timestamp, bounds, data);
      int y = windArea.y + windArea.height / 2;

      DrawWindArrow(dc, x, y, point.windSpeed, point.windDirection,
                    windArea.height / 3);

      // Draw wind speed value
      double userWindSpeed = toUsrWindSpeed_Plugin(point.windSpeed);
      wxString windStr = wxString::Format("%.0f", userWindSpeed);
      wxSize textSize = dc.GetTextExtent(windStr);
      dc.SetTextForeground(GetWindArrowColor(point.windSpeed));
      dc.DrawText(windStr, x - textSize.x / 2,
                  windArea.y + windArea.height - textSize.y);
    }
  }

  // Draw wind axis label
  dc.SetFont(m_labelFont);
  dc.SetTextForeground(m_windColor);
  auto [minWind, maxWind] = data.GetWindSpeedRange();
  double userMaxWind = toUsrWindSpeed_Plugin(maxWind);
  wxString windUnit = getUsrWindSpeedUnit_Plugin();
  wxString windLabel =
      wxString::Format("Wind (0 - %.0f %s)", userMaxWind, windUnit);
  dc.DrawText(windLabel, windArea.x, windArea.y - 20);
}

void MeteogramRenderer::DrawPressureLayer(wxDC& dc, const wxRect& bounds,
                                          const MeteogramData& data) {
  if (!m_showPressure) return;

  wxRect pressArea = GetPressureArea(bounds);
  auto [minPress, maxPress] = data.GetPressureRange();

  const auto& points = data.GetDataPoints();
  std::vector<wxPoint> pressPoints;

  // Collect pressure curve points
  for (const auto& point : points) {
    if (point.pressure != GRIB_NOTDEF) {
      int x = TimeToPixel(point.timestamp, bounds, data);
      int y = ValueToPixel(point.pressure, minPress, maxPress, pressArea);
      pressPoints.push_back(wxPoint(x, y));
    }
  }

  if (pressPoints.size() > 1) {
    // Draw pressure curve
    dc.SetPen(wxPen(m_pressureColor, 2));
    dc.DrawLines(pressPoints.size(), pressPoints.data());

    // Draw pressure values at key points
    dc.SetFont(m_valueFont);
    dc.SetTextForeground(m_pressureColor);

    for (size_t i = 0; i < pressPoints.size(); i += 4) {  // Every 4th point
      if (i < points.size() && points[i].pressure != GRIB_NOTDEF) {
        wxString pressStr = wxString::Format("%.0f", points[i].pressure);
        wxSize textSize = dc.GetTextExtent(pressStr);
        dc.DrawText(pressStr, pressPoints[i].x - textSize.x / 2,
                    pressPoints[i].y - textSize.y - 2);
      }
    }
  }

  // Draw pressure axis label
  dc.SetFont(m_labelFont);
  dc.SetTextForeground(m_pressureColor);
  wxString pressLabel =
      wxString::Format("Pressure (%.0f - %.0f hPa)", minPress, maxPress);
  dc.DrawText(pressLabel, pressArea.x, pressArea.y - 20);
}

void MeteogramRenderer::DrawPrecipitationLayer(wxDC& dc, const wxRect& bounds,
                                               const MeteogramData& data) {
  if (!m_showPrecipitation) return;

  wxRect precipArea = GetPrecipitationArea(bounds);
  const auto& points = data.GetDataPoints();

  // Find max precipitation for scaling
  double maxPrecip = 0.0;
  for (const auto& point : points) {
    if (point.precipitation != GRIB_NOTDEF) {
      maxPrecip = std::max(maxPrecip, point.precipitation);
    }
  }

  if (maxPrecip <= 0.0) return;

  dc.SetBrush(wxBrush(m_precipitationColor));
  dc.SetPen(wxPen(m_precipitationColor, 1));

  // Draw precipitation bars
  int barWidth =
      std::max(2, bounds.width / static_cast<int>(points.size()) - 1);

  for (const auto& point : points) {
    if (point.precipitation != GRIB_NOTDEF && point.precipitation > 0.0) {
      int x = TimeToPixel(point.timestamp, bounds, data);
      int barHeight =
          static_cast<int>(point.precipitation / maxPrecip * precipArea.height);

      wxRect barRect(x - barWidth / 2,
                     precipArea.y + precipArea.height - barHeight, barWidth,
                     barHeight);
      dc.DrawRectangle(barRect);
    }
  }

  // Draw precipitation axis label
  dc.SetFont(m_labelFont);
  dc.SetTextForeground(m_precipitationColor);
  wxString precipLabel =
      wxString::Format("Precipitation (0 - %.1f mm/h)", maxPrecip);
  dc.DrawText(precipLabel, precipArea.x, precipArea.y - 20);
}

void MeteogramRenderer::DrawWaveLayer(wxDC& dc, const wxRect& bounds,
                                      const MeteogramData& data) {
  if (!m_showWaves) return;

  wxRect waveArea = GetWaveArea(bounds);
  const auto& points = data.GetDataPoints();

  // Find max wave height for scaling
  double maxWave = 0.0;
  for (const auto& point : points) {
    if (point.waveHeight != GRIB_NOTDEF) {
      maxWave = std::max(maxWave, point.waveHeight);
    }
  }

  if (maxWave <= 0.0) return;

  std::vector<wxPoint> wavePoints;

  // Collect wave height curve points
  for (const auto& point : points) {
    if (point.waveHeight != GRIB_NOTDEF) {
      int x = TimeToPixel(point.timestamp, bounds, data);
      int y = ValueToPixel(point.waveHeight, 0.0, maxWave, waveArea);
      wavePoints.push_back(wxPoint(x, y));
    }
  }

  if (wavePoints.size() > 1) {
    // Draw wave height curve
    dc.SetPen(wxPen(m_waveColor, 2));
    dc.DrawLines(wavePoints.size(), wavePoints.data());
  }

  // Draw wave axis label
  dc.SetFont(m_labelFont);
  dc.SetTextForeground(m_waveColor);
  wxString waveLabel = wxString::Format("Wave Height (0 - %.1f m)", maxWave);
  dc.DrawText(waveLabel, waveArea.x, waveArea.y - 20);
}

void MeteogramRenderer::DrawSelectedTimeIndicator(
    wxDC& dc, const wxRect& bounds, const MeteogramData& data,
    const wxDateTime& selectedTime) {
  int x = TimeToPixel(selectedTime, bounds, data);

  // Draw vertical line
  dc.SetPen(wxPen(*wxRED, 2));
  dc.DrawLine(x, bounds.y, x, bounds.y + bounds.height);

  // Draw triangular indicators at top and bottom
  dc.SetBrush(wxBrush(*wxRED));
  dc.SetPen(wxPen(*wxRED, 1));

  // Top triangle - tip pointing down
  wxPoint topTriangle[3];
  topTriangle[0] = wxPoint(x, bounds.y + 6);  // Tip
  topTriangle[1] = wxPoint(x - 4, bounds.y);  // Left
  topTriangle[2] = wxPoint(x + 4, bounds.y);  // Right
  dc.DrawPolygon(3, topTriangle);

  // Bottom triangle - tip pointing up
  wxPoint bottomTriangle[3];
  bottomTriangle[0] = wxPoint(x, bounds.y + bounds.height - 6);  // Tip
  bottomTriangle[1] = wxPoint(x - 4, bounds.y + bounds.height);  // Left
  bottomTriangle[2] = wxPoint(x + 4, bounds.y + bounds.height);  // Right
  dc.DrawPolygon(3, bottomTriangle);
}

//=============================================================================
// Layout Helper Methods
//=============================================================================

wxRect MeteogramRenderer::GetTimeAxisArea(const wxRect& bounds) const {
  return wxRect(bounds.x, bounds.y + bounds.height - 40, bounds.width, 40);
}

wxRect MeteogramRenderer::GetTemperatureArea(const wxRect& bounds) const {
  int y = bounds.y + 20;
  int height = 80;
  return wxRect(bounds.x, y, bounds.width, height);
}

wxRect MeteogramRenderer::GetWindArea(const wxRect& bounds) const {
  int y = bounds.y + 20;
  if (m_showTemperature) y += 80;
  int height = 60;
  return wxRect(bounds.x, y, bounds.width, height);
}

wxRect MeteogramRenderer::GetPressureArea(const wxRect& bounds) const {
  int y = bounds.y + 20;
  if (m_showTemperature) y += 80;
  if (m_showWind) y += 60;
  int height = 80;
  return wxRect(bounds.x, y, bounds.width, height);
}

wxRect MeteogramRenderer::GetPrecipitationArea(const wxRect& bounds) const {
  int y = bounds.y + 20;
  if (m_showTemperature) y += 80;
  if (m_showWind) y += 60;
  if (m_showPressure) y += 80;
  int height = 40;
  return wxRect(bounds.x, y, bounds.width, height);
}

wxRect MeteogramRenderer::GetWaveArea(const wxRect& bounds) const {
  int y = bounds.y + 20;
  if (m_showTemperature) y += 80;
  if (m_showWind) y += 60;
  if (m_showPressure) y += 80;
  if (m_showPrecipitation) y += 40;
  int height = 60;
  return wxRect(bounds.x, y, bounds.width, height);
}

//=============================================================================
// Coordinate Conversion Methods
//=============================================================================

int MeteogramRenderer::TimeToPixel(const wxDateTime& time, const wxRect& bounds,
                                   const MeteogramData& data) const {
  if (!data.IsValid()) return bounds.x;

  wxTimeSpan totalDuration = data.GetEndTime() - data.GetStartTime();
  wxTimeSpan timeOffset = time - data.GetStartTime();

  if (totalDuration.GetSeconds() == 0) return bounds.x;

  double fraction = timeOffset.GetSeconds().ToDouble() /
                    totalDuration.GetSeconds().ToDouble();
  return bounds.x + static_cast<int>(fraction * bounds.width);
}

wxDateTime MeteogramRenderer::PixelToTime(int x, const wxRect& bounds,
                                          const MeteogramData& data) const {
  if (!data.IsValid()) return wxInvalidDateTime;

  double fraction = static_cast<double>(x - bounds.x) / bounds.width;
  wxTimeSpan totalDuration = data.GetEndTime() - data.GetStartTime();
  wxTimeSpan offset =
      wxTimeSpan::Seconds(totalDuration.GetSeconds().ToLong() * fraction);

  return data.GetStartTime() + offset;
}

double MeteogramRenderer::ValueToPixel(double value, double minVal,
                                       double maxVal, const wxRect& area,
                                       bool invertY) const {
  if (maxVal == minVal) return area.y + area.height / 2;

  double fraction = (value - minVal) / (maxVal - minVal);

  if (invertY) {
    return area.y + area.height - (fraction * area.height);
  } else {
    return area.y + (fraction * area.height);
  }
}

//=============================================================================
// Wind Arrow and Styling Methods
//=============================================================================

void MeteogramRenderer::DrawWindArrow(wxDC& dc, int x, int y, double speed,
                                      double direction, int maxSize) {
  // Wind direction convention: direction wind is coming FROM (meteorological)
  double angleRad =
      (direction + 180) * M_PI / 180.0;  // Convert to direction going TO

  // Arrow length based on wind speed
  int arrowLength = static_cast<int>(speed * 2);  // 2 pixels per knot
  if (arrowLength > maxSize) arrowLength = maxSize;
  if (arrowLength < 5) arrowLength = 5;  // Minimum arrow size

  // Calculate arrow end point
  int endX = x + static_cast<int>(arrowLength * sin(angleRad));
  int endY = y - static_cast<int>(arrowLength * cos(angleRad));

  // Set arrow color based on wind speed
  wxColour arrowColor = GetWindArrowColor(speed);
  dc.SetPen(wxPen(arrowColor, 2));
  dc.SetBrush(wxBrush(arrowColor));

  // Draw arrow shaft
  dc.DrawLine(x, y, endX, endY);

  // Draw arrowhead
  double arrowAngle = M_PI / 6;  // 30 degrees
  int arrowheadLength = 8;

  int ah1X =
      endX - static_cast<int>(arrowheadLength * sin(angleRad - arrowAngle));
  int ah1Y =
      endY + static_cast<int>(arrowheadLength * cos(angleRad - arrowAngle));
  int ah2X =
      endX - static_cast<int>(arrowheadLength * sin(angleRad + arrowAngle));
  int ah2Y =
      endY + static_cast<int>(arrowheadLength * cos(angleRad + arrowAngle));

  dc.DrawLine(endX, endY, ah1X, ah1Y);
  dc.DrawLine(endX, endY, ah2X, ah2Y);

  // Draw wind barbs for higher speeds
  if (speed >= 10) {
    int barbSpacing = arrowLength / 4;
    int barbLength = 6;

    for (int i = 1; speed >= 10 * i && i <= 4; i++) {
      int barbX = x + static_cast<int>((barbSpacing * i) * sin(angleRad));
      int barbY = y - static_cast<int>((barbSpacing * i) * cos(angleRad));

      int barbEndX =
          barbX + static_cast<int>(barbLength * sin(angleRad + M_PI / 2));
      int barbEndY =
          barbY - static_cast<int>(barbLength * cos(angleRad + M_PI / 2));

      dc.DrawLine(barbX, barbY, barbEndX, barbEndY);
    }
  }
}

wxColour MeteogramRenderer::GetWindArrowColor(double windSpeed) const {
  // Color code wind arrows by speed (Beaufort scale inspired)
  if (windSpeed < 5)
    return wxColour(100, 200, 100);  // Light green - light winds
  else if (windSpeed < 12)
    return wxColour(100, 150, 200);  // Blue - moderate winds
  else if (windSpeed < 20)
    return wxColour(200, 200, 100);  // Yellow - fresh winds
  else if (windSpeed < 30)
    return wxColour(200, 150, 100);  // Orange - strong winds
  else
    return wxColour(200, 100, 100);  // Red - gale+ winds
}

wxColour MeteogramRenderer::GetTemperatureColor(double temperature) const {
  // Color code temperature values
  if (temperature < 0)
    return wxColour(100, 100, 255);  // Blue - freezing
  else if (temperature < 10)
    return wxColour(100, 200, 255);  // Light blue - cold
  else if (temperature < 25)
    return wxColour(200, 200, 100);  // Green/yellow - moderate
  else
    return wxColour(255, 100, 100);  // Red - hot
}

//=============================================================================
// Mouse Interaction Methods
//=============================================================================

wxDateTime MeteogramRenderer::GetTimeAtPosition(
    const wxPoint& mousePos, const wxRect& bounds,
    const MeteogramData& data) const {
  return PixelToTime(mousePos.x, bounds, data);
}

wxString MeteogramRenderer::GetTooltipAtPosition(
    const wxPoint& mousePos, const wxRect& bounds,
    const MeteogramData& data) const {
  wxDateTime timeAtMouse = GetTimeAtPosition(mousePos, bounds, data);
  if (!timeAtMouse.IsValid()) return wxEmptyString;

  MeteogramDataPoint point = data.GetDataAtTime(timeAtMouse);
  if (!point.IsValid()) return wxEmptyString;

  wxString tooltip = point.timestamp.Format("%Y-%m-%d %H:%M UTC\n");

  if (point.temperature != GRIB_NOTDEF) {
    double userTemp = toUsrTemp_Plugin(point.temperature);
    wxString tempUnit = getUsrTempUnit_Plugin();
    tooltip += wxString::Format("Temperature: %.1f%s\n", userTemp, tempUnit);
  }
  if (point.windSpeed != GRIB_NOTDEF) {
    double userWindSpeed = toUsrWindSpeed_Plugin(point.windSpeed);
    wxString windUnit = getUsrWindSpeedUnit_Plugin();
    tooltip += wxString::Format("Wind: %.1f %s", userWindSpeed, windUnit);
    if (point.windDirection != GRIB_NOTDEF) {
      tooltip += wxString::Format(" from %.0f°", point.windDirection);
    }
    tooltip += "\n";
  }
  if (point.pressure != GRIB_NOTDEF) {
    tooltip += wxString::Format("Pressure: %.1f hPa\n", point.pressure);
  }
  if (point.precipitation != GRIB_NOTDEF && point.precipitation > 0.0) {
    tooltip +=
        wxString::Format("Precipitation: %.1f mm/h\n", point.precipitation);
  }
  if (point.waveHeight != GRIB_NOTDEF) {
    tooltip += wxString::Format("Wave Height: %.1f m\n", point.waveHeight);
  }

  return tooltip.Trim();
}
