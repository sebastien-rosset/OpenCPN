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
#include "GribUIDialog.h"
#include "grib_pi.h"

#include <wx/dcbuffer.h>
#include <wx/graphics.h>
#include <algorithm>
#include <cmath>
#include <limits>

// Custom events
wxDEFINE_EVENT(EVT_METEOGRAM_TIME_SELECTED, wxCommandEvent);
wxDEFINE_EVENT(EVT_METEOGRAM_LOCATION_CHANGED, wxCommandEvent);

//=============================================================================
// MeteogramDataPoint Implementation
//=============================================================================

bool MeteogramDataPoint::IsValid() const {
  return timestamp.IsValid() &&
         (temperature != GRIB_NOTDEF || windSpeed != GRIB_NOTDEF ||
          pressure != GRIB_NOTDEF || precipitation != GRIB_NOTDEF);
}

//=============================================================================
// MeteogramLocation Implementation
//=============================================================================

MeteogramLocation::MeteogramLocation(double lat, double lon,
                                     const wxString& name)
    : m_type(kPoint), m_name(name), m_lat(lat), m_lon(lon) {
  if (m_name.IsEmpty()) {
    m_name = wxString::Format("%.3f°, %.3f°", lat, lon);
  }
}

MeteogramLocation::MeteogramLocation(
    const std::vector<std::pair<double, double>>& routePoints,
    const wxString& routeName)
    : m_type(kRoute),
      m_name(routeName),
      m_lat(0),
      m_lon(0),
      m_routePoints(routePoints) {
  if (m_name.IsEmpty()) {
    m_name = wxString::Format("Route (%zu points)", routePoints.size());
  }
}

wxString MeteogramLocation::GetDisplayName() const {
  if (m_type == kPoint) {
    return wxString::Format("%s (%.3f°, %.3f°)", m_name, m_lat, m_lon);
  } else {
    return wxString::Format("%s (%zu waypoints)", m_name, m_routePoints.size());
  }
}

std::pair<double, double> MeteogramLocation::GetRoutePointAt(
    double fraction) const {
  if (m_type != kRoute || m_routePoints.empty()) {
    return std::make_pair(m_lat, m_lon);
  }

  if (fraction <= 0.0) return m_routePoints.front();
  if (fraction >= 1.0) return m_routePoints.back();

  // Linear interpolation along route
  double segmentLength = 1.0 / (m_routePoints.size() - 1);
  int segmentIndex = static_cast<int>(fraction / segmentLength);
  double segmentFraction =
      (fraction - segmentIndex * segmentLength) / segmentLength;

  if (segmentIndex >= static_cast<int>(m_routePoints.size() - 1)) {
    return m_routePoints.back();
  }

  const auto& p1 = m_routePoints[segmentIndex];
  const auto& p2 = m_routePoints[segmentIndex + 1];

  double lat = p1.first + (p2.first - p1.first) * segmentFraction;
  double lon = p1.second + (p2.second - p1.second) * segmentFraction;

  return std::make_pair(lat, lon);
}

//=============================================================================
// MeteogramData Implementation
//=============================================================================

MeteogramData::MeteogramData() : m_location(0, 0) {}

void MeteogramData::Clear() {
  m_dataPoints.clear();
  m_startTime = wxInvalidDateTime;
  m_endTime = wxInvalidDateTime;
}

void MeteogramData::LoadFromGribRecordSets(ArrayOfGribRecordSets* recordSets,
                                           const MeteogramLocation& location) {
  Clear();
  m_location = location;

  if (!recordSets || recordSets->GetCount() == 0) return;

  if (location.GetType() == MeteogramLocation::kPoint) {
    ExtractPointData(recordSets, location.GetLatitude(),
                     location.GetLongitude());
  } else {
    ExtractRouteData(recordSets, location.GetRoutePoints());
  }

  if (!m_dataPoints.empty()) {
    m_startTime = m_dataPoints.front().timestamp;
    m_endTime = m_dataPoints.back().timestamp;
  }
}

void MeteogramData::ExtractPointData(ArrayOfGribRecordSets* recordSets,
                                     double lat, double lon) {
  for (unsigned i = 0; i < recordSets->GetCount(); i++) {
    GribRecordSet* timeSet = &recordSets->Item(i);
    if (!timeSet) continue;

    GribRecord** recordArray = timeSet->m_GribRecordPtrArray;
    if (!recordArray) continue;

    MeteogramDataPoint point = ExtractDataPoint(recordArray, lat, lon);
    point.timestamp = timeSet->m_Reference_Time;

    if (point.IsValid()) {
      m_dataPoints.push_back(point);
    }
  }
}

void MeteogramData::ExtractRouteData(
    ArrayOfGribRecordSets* recordSets,
    const std::vector<std::pair<double, double>>& routePoints) {
  if (routePoints.empty()) return;

  for (unsigned i = 0; i < recordSets->GetCount(); i++) {
    GribRecordSet* timeSet = &recordSets->Item(i);
    if (!timeSet) continue;

    GribRecord** recordArray = timeSet->m_GribRecordPtrArray;
    if (!recordArray) continue;

    // For routes, we sample multiple points and average them
    // This gives a representative view of conditions along the route
    MeteogramDataPoint avgPoint;
    avgPoint.timestamp = timeSet->m_Reference_Time;

    int validSamples = 0;
    double tempSum = 0, pressSum = 0, windSpeedSum = 0, windDirSum = 0;
    double precipSum = 0, humidSum = 0, cloudSum = 0;
    double waveHeightSum = 0, wavePeriodSum = 0, waveDirSum = 0;

    // Sample every few points along the route.
    int sampleStep = std::max(1, static_cast<int>(routePoints.size() / 10));

    for (size_t j = 0; j < routePoints.size(); j += sampleStep) {
      const auto& point = routePoints[j];
      MeteogramDataPoint samplePoint =
          ExtractDataPoint(recordArray, point.first, point.second);

      if (samplePoint.IsValid()) {
        validSamples++;
        if (samplePoint.temperature != GRIB_NOTDEF)
          tempSum += samplePoint.temperature;
        if (samplePoint.pressure != GRIB_NOTDEF)
          pressSum += samplePoint.pressure;
        if (samplePoint.windSpeed != GRIB_NOTDEF)
          windSpeedSum += samplePoint.windSpeed;
        if (samplePoint.windDirection != GRIB_NOTDEF)
          windDirSum += samplePoint.windDirection;
        if (samplePoint.precipitation != GRIB_NOTDEF)
          precipSum += samplePoint.precipitation;
        if (samplePoint.humidity != GRIB_NOTDEF)
          humidSum += samplePoint.humidity;
        if (samplePoint.cloudCover != GRIB_NOTDEF)
          cloudSum += samplePoint.cloudCover;
        if (samplePoint.waveHeight != GRIB_NOTDEF)
          waveHeightSum += samplePoint.waveHeight;
        if (samplePoint.wavePeriod != GRIB_NOTDEF)
          wavePeriodSum += samplePoint.wavePeriod;
        if (samplePoint.waveDirection != GRIB_NOTDEF)
          waveDirSum += samplePoint.waveDirection;
      }
    }

    if (validSamples > 0) {
      avgPoint.temperature = tempSum / validSamples;
      avgPoint.pressure = pressSum / validSamples;
      avgPoint.windSpeed = windSpeedSum / validSamples;
      avgPoint.windDirection =
          windDirSum /
          validSamples;  // Note: circular averaging would be more accurate
      avgPoint.precipitation = precipSum / validSamples;
      avgPoint.humidity = humidSum / validSamples;
      avgPoint.cloudCover = cloudSum / validSamples;
      avgPoint.waveHeight = waveHeightSum / validSamples;
      avgPoint.wavePeriod = wavePeriodSum / validSamples;
      avgPoint.waveDirection = waveDirSum / validSamples;

      m_dataPoints.push_back(avgPoint);
    }
  }
}

MeteogramDataPoint MeteogramData::ExtractDataPoint(GribRecord** recordArray,
                                                   double lat, double lon) {
  MeteogramDataPoint point;

  // Extract wind data (same logic as GRIBTable::GetWind)
  double vkn, ang;
  if (GribRecord::getInterpolatedValues(vkn, ang, recordArray[Idx_WIND_VX],
                                        recordArray[Idx_WIND_VY], lon, lat)) {
    point.windSpeed = vkn;
    point.windDirection = ang;
  }

  // Extract temperature (same logic as GRIBTable::GetAirTemp)
  if (recordArray[Idx_AIR_TEMP]) {
    point.temperature =
        recordArray[Idx_AIR_TEMP]->getInterpolatedValue(lon, lat, true);
  }

  // Extract pressure (same logic as GRIBTable::GetPressure)
  if (recordArray[Idx_PRESSURE]) {
    point.pressure =
        recordArray[Idx_PRESSURE]->getInterpolatedValue(lon, lat, true);
  }

  // Extract precipitation (same logic as GRIBTable::GetRainfall)
  if (recordArray[Idx_PRECIP_TOT]) {
    point.precipitation =
        recordArray[Idx_PRECIP_TOT]->getInterpolatedValue(lon, lat, true);
  }

  // Extract humidity
  if (recordArray[Idx_HUMID_RE]) {
    point.humidity =
        recordArray[Idx_HUMID_RE]->getInterpolatedValue(lon, lat, true);
  }

  // Extract cloud cover
  if (recordArray[Idx_CLOUD_TOT]) {
    point.cloudCover =
        recordArray[Idx_CLOUD_TOT]->getInterpolatedValue(lon, lat, true);
  }

  // Extract wind gust
  if (recordArray[Idx_WIND_GUST]) {
    point.windGust =
        recordArray[Idx_WIND_GUST]->getInterpolatedValue(lon, lat, true);
  }

  // Extract wave data
  if (recordArray[Idx_HTSIGW]) {
    point.waveHeight =
        recordArray[Idx_HTSIGW]->getInterpolatedValue(lon, lat, true);
  }
  if (recordArray[Idx_WVPER]) {
    point.wavePeriod =
        recordArray[Idx_WVPER]->getInterpolatedValue(lon, lat, true);
  }
  if (recordArray[Idx_WVDIR]) {
    point.waveDirection =
        recordArray[Idx_WVDIR]->getInterpolatedValue(lon, lat, true);
  }

  return point;
}

MeteogramDataPoint MeteogramData::GetDataAtTime(const wxDateTime& time) const {
  if (m_dataPoints.empty()) return MeteogramDataPoint();

  // Find closest data point
  auto it = std::min_element(
      m_dataPoints.begin(), m_dataPoints.end(),
      [&time](const MeteogramDataPoint& a, const MeteogramDataPoint& b) {
        wxTimeSpan diffA = time - a.timestamp;
        wxTimeSpan diffB = time - b.timestamp;
        return abs(diffA.GetSeconds().ToLong()) <
               abs(diffB.GetSeconds().ToLong());
      });

  return *it;
}

std::pair<double, double> MeteogramData::GetTemperatureRange() const {
  if (m_dataPoints.empty()) return std::make_pair(0.0, 30.0);

  double minTemp = 1000.0, maxTemp = -1000.0;
  bool hasData = false;

  for (const auto& point : m_dataPoints) {
    if (point.temperature != GRIB_NOTDEF) {
      minTemp = std::min(minTemp, point.temperature);
      maxTemp = std::max(maxTemp, point.temperature);
      hasData = true;
    }
  }

  if (!hasData) return std::make_pair(0.0, 30.0);

  // Add some padding
  double range = maxTemp - minTemp;
  if (range < 5.0) range = 5.0;  // Minimum range
  double padding = range * 0.1;

  return std::make_pair(minTemp - padding, maxTemp + padding);
}

std::pair<double, double> MeteogramData::GetPressureRange() const {
  if (m_dataPoints.empty()) return std::make_pair(980.0, 1040.0);

  double minPress = 2000.0, maxPress = 0.0;
  bool hasData = false;

  for (const auto& point : m_dataPoints) {
    if (point.pressure != GRIB_NOTDEF) {
      minPress = std::min(minPress, point.pressure);
      maxPress = std::max(maxPress, point.pressure);
      hasData = true;
    }
  }

  if (!hasData) return std::make_pair(980.0, 1040.0);

  // Add some padding
  double range = maxPress - minPress;
  if (range < 20.0) range = 20.0;  // Minimum range
  double padding = range * 0.1;

  return std::make_pair(minPress - padding, maxPress + padding);
}

std::pair<double, double> MeteogramData::GetWindSpeedRange() const {
  if (m_dataPoints.empty()) return std::make_pair(0.0, 30.0);

  double maxWind = 0.0;
  bool hasData = false;

  for (const auto& point : m_dataPoints) {
    if (point.windSpeed != GRIB_NOTDEF) {
      maxWind = std::max(maxWind, point.windSpeed);
      hasData = true;
    }
  }

  if (!hasData) return std::make_pair(0.0, 30.0);

  // Round up to nice values
  if (maxWind < 10)
    maxWind = 10;
  else if (maxWind < 20)
    maxWind = 20;
  else if (maxWind < 30)
    maxWind = 30;
  else if (maxWind < 50)
    maxWind = 50;
  else
    maxWind = ceil(maxWind / 10) * 10;

  return std::make_pair(0.0, maxWind);
}
