/***************************************************************************
 *   Copyright (C) 2024 by OpenCPN Development Team                        *
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
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 **************************************************************************/

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif  // precompiled headers
#include <wx/log.h>

#include "grib_layer_set.h"
#include "GribRecord.h"
#include "GribRecordSet.h"

wxString GRIBLayerSet::GetLastMessage() const { return m_LastMessage; }

GRIBFile::GribIdxArray& GRIBLayerSet::GetGribIdxArray() {
  return m_gribIdxArray;
}

void GRIBLayerSet::UpdateIdxArrayCache() {
  // Merge indices from all enabled layers.
  // The packing scheme guarantees unique integers for different parameter/level
  // combinations.
  m_gribIdxArray.Clear();
  for (const auto& pair : m_layers) {
    const GRIBLayer* layer = pair.second.get();
    if (layer->IsEnabled() && layer->IsOK()) {
      const auto& layerIndices = layer->GetFile()->GetGribIdxArray();
      // Add unique indices
      for (const auto& idx : layerIndices) {
        if (m_gribIdxArray.Index(idx) == wxNOT_FOUND) {
          m_gribIdxArray.Add(idx);
        }
      }
    }
  }
}

void GRIBLayerSet::UpdateCache() {
  UpdateIdxArrayCache();
  // Update cached filenames
  UpdateFileNameCache();
  UpdateLayerOrder();
}

bool GRIBLayerSet::IsOK() const {
  // Return true if at least one enabled layer is OK
  for (const auto& pair : m_layers) {
    const GRIBLayer* layer = pair.second.get();
    if (layer->IsEnabled() && layer->IsOK()) {
      return true;
    }
  }
  return false;
}

const wxArrayString& GRIBLayerSet::GetFileNames() const { return m_fileNames; }

const ArrayOfGribRecordSets* GRIBLayerSet::GetRecordSetArrayPtr() const {
  for (auto& pair : m_layers) {
    auto& layer = pair.second;
    if (!layer || !layer->IsEnabled()) {
      continue;
    }
    wxLogMessage("GRIBLayerSet::GetRecordSetArrayPtr: Found enabled layer: %s",
                 layer->GetName());
    return layer->GetFile()->GetRecordSetArrayPtr();
  }
  wxLogMessage("GRIBLayerSet::GetRecordSetArrayPtr: No enabled layers found.");
  return nullptr;
}

// @todo: need lat/lon arguments because for each layer, we need to check if the
// point is within the grid bounds.
GribRecord* GRIBLayerSet::findBestRecord(int recordIndex,
                                         const wxDateTime* time) const {
  // Collect available GribRecords from each layer's timeline
  std::vector<GribRecord*> records;

  if (time == nullptr) {
    time = &m_selectedTime;
  }
  // Get interpolated values from each enabled layer that has data
  for (auto& pair : m_layers) {
    auto& layer = pair.second;
    if (!layer || !layer->IsEnabled()) {
      continue;
    }

    GribTimelineRecordSet* timelineSet = layer->GetTimeLineRecordSet(*time);
    if (!timelineSet) continue;

    GribRecord* record = timelineSet->GetRecord(recordIndex);
    if (record) {
      records.push_back(record);
    }
  }

  // Score the records using merge strategy.
  std::vector<double> scores =
      m_mergeStrategy.ScoreRecords(records, time->GetTicks());

  // Find record with best score.
  GribRecord* bestRecord = nullptr;
  double bestScore = -1;
  for (size_t i = 0; i < scores.size(); i++) {
    if (scores[i] > bestScore) {
      bestScore = scores[i];
      bestRecord = records[i];
    }
  }
  return bestRecord;
}

double GRIBLayerSet::GetInterpolatedValue(int recordIndex, double lon,
                                          double lat, const wxDateTime* time,
                                          bool numericalInterpolation) const {
  GribRecord* bestRecord = findBestRecord(recordIndex, time);
  if (!bestRecord) {
    return GRIB_NOTDEF;
  }
  // Currently only wave direction is stored as a directional value.
  bool polar = recordIndex == Idx_WVDIR;

  // Use best record to get interpolated value
  return bestRecord->getInterpolatedValue(lon, lat, numericalInterpolation,
                                          polar);
}

// @todo: need lat/lon arguments because for each layer, we need to check if the
// point is within the grid bounds.
std::pair<GribRecord*, GribRecord*> GRIBLayerSet::findBestRecordVector(
    int xComponentIndex, int yComponentIndex, double lon, double lat,
    const wxDateTime* time) const {
  // Collect available vector component pairs from each layer's timeline
  std::vector<std::pair<GribRecord*, GribRecord*>> recordPairs;
  std::vector<GribRecord*> validRecords;  // For scoring

  if (time == nullptr) {
    time = &m_selectedTime;
  }
  // Get interpolated values from each enabled layer that has data
  for (auto& pair : m_layers) {
    auto& layer = pair.second;
    if (!layer || !layer->IsEnabled()) {
      continue;
    }

    GribTimelineRecordSet* timelineSet = layer->GetTimeLineRecordSet(*time);
    if (!timelineSet) continue;

    GribRecord* xRecord = timelineSet->GetRecord(xComponentIndex);
    GribRecord* yRecord = timelineSet->GetRecord(yComponentIndex);

    // Only include layers that have both components
    if (xRecord && yRecord) {
      recordPairs.push_back(std::make_pair(xRecord, yRecord));
      validRecords.push_back(xRecord);  // Score using X component
    }
  }

  // Score the records using merge strategy
  std::vector<double> scores =
      m_mergeStrategy.ScoreRecords(validRecords, time->GetTicks());

  // Find pair with best score
  std::pair<GribRecord*, GribRecord*> bestPair(nullptr, nullptr);
  double bestScore = -1;
  for (size_t i = 0; i < scores.size(); i++) {
    if (scores[i] > bestScore) {
      bestScore = scores[i];
      bestPair = recordPairs[i];
    }
  }

  return bestPair;
}

bool GRIBLayerSet::GetInterpolatedVector(double& magnitude, double& angle,
                                         int xComponentIndex,
                                         int yComponentIndex, double lon,
                                         double lat, const wxDateTime* time,
                                         bool numericalInterpolation) const {
  // Get best matching pair of vector components from same layer
  auto [GRX, GRY] =
      findBestRecordVector(xComponentIndex, yComponentIndex, lon, lat, time);

  // Need both components for vector interpolation
  if (!GRX || !GRY) {
    return false;
  }
  // Use best record to get interpolated value
  return GribRecord::getInterpolatedValues(magnitude, angle, GRX, GRY, lon, lat,
                                           numericalInterpolation);
}

time_t GRIBLayerSet::GetRefDateTime() const {
  // Return the newest reference time from all enabled layers
  time_t newest = 0;
  for (const auto& pair : m_layers) {
    const GRIBLayer* layer = pair.second.get();
    if (layer->IsEnabled() && layer->IsOK()) {
      time_t layerTime = layer->GetFile()->GetRefDateTime();
      if (layerTime > newest) {
        newest = layerTime;
      }
    }
  }
  return newest;
}

GRIBLayer* GRIBLayerSet::AddLayer(const wxString& name, const wxString& file,
                                  bool copyFirstCumRec, bool copyMissWaveRec,
                                  bool newestFile) {
  // Don't allow duplicate layer names
  if (m_layers.find(name) != m_layers.end()) {
    m_LastMessage = wxString::Format(_("Layer '%s' already exists"), name);
    return nullptr;
  }

  if (!::wxFileExists(file)) {
    m_LastMessage =
        wxString::Format(_("File for layer '%s' does not exist"), name);
    return nullptr;
  }
  wxArrayString files;
  files.Add(file);

  // Create and configure the GRIBFile.
  // Note: newestFile logic will need to be exposed as a parameter if needed
  GRIBFile* gribFile =
      new GRIBFile(files, copyFirstCumRec, copyMissWaveRec, newestFile);

  if (!gribFile->IsOK()) {
    m_LastMessage = wxString::Format(_("Layer '%s' can't be read"), name);
    delete gribFile;
    return nullptr;
  }

  const ArrayOfGribRecordSets* rsa = gribFile->GetRecordSetArrayPtr();
  if (rsa->GetCount() == 0) {
    m_LastMessage =
        wxString::Format(_("Layer '%s' contains no valid data"), name);
    delete gribFile;
    return nullptr;
  }

  // Create new layer with the valid GRIBFile
  auto layer = std::make_unique<GRIBLayer>(name, gribFile, this);
  GRIBLayer* layerPtr = layer.get();
  m_layers[name] = std::move(layer);

  // Add to layer order.
  m_layerOrder.push_back(name);

  UpdateCache();

  return layerPtr;
}

bool GRIBLayerSet::RemoveLayer(const wxString& name) {
  auto it = m_layers.find(name);
  if (it != m_layers.end()) {
    // Remove from layer order vector
    auto orderIt = std::find(m_layerOrder.begin(), m_layerOrder.end(), name);
    if (orderIt != m_layerOrder.end()) {
      m_layerOrder.erase(orderIt);
    }

    // Remove from layers map
    m_layers.erase(it);

    UpdateCache();
    return true;
  }
  return false;
}

void GRIBLayerSet::ClearLayers() {
  m_layers.clear();
  m_layerOrder.clear();
  UpdateCache();
}

void GRIBLayerSet::UpdateFileNameCache() {
  m_fileNames.Clear();
  for (const auto& pair : m_layers) {
    const GRIBLayer* layer = pair.second.get();
    if (layer->IsEnabled() && layer->IsOK()) {
      const wxArrayString& layerFiles = layer->GetFile()->GetFileNames();
      for (const auto& file : layerFiles) {
        m_fileNames.Add(file);
      }
    }
  }
}

GRIBLayer* GRIBLayerSet::GetLayer(const wxString& name) {
  auto it = m_layers.find(name);
  return (it != m_layers.end()) ? it->second.get() : nullptr;
}

const GRIBLayer* GRIBLayerSet::GetLayer(const wxString& name) const {
  auto it = m_layers.find(name);
  return (it != m_layers.end()) ? it->second.get() : nullptr;
}

bool GRIBLayerSet::MoveLayerUp(const wxString& name) {
  auto it = std::find(m_layerOrder.begin(), m_layerOrder.end(), name);
  if (it == m_layerOrder.begin() || it == m_layerOrder.end()) {
    return false;  // Already at top or not found
  }

  // Swap with previous element
  std::iter_swap(it, it - 1);
  UpdateCache();
  return true;
}

bool GRIBLayerSet::MoveLayerDown(const wxString& name) {
  auto it = std::find(m_layerOrder.begin(), m_layerOrder.end(), name);
  if (it == m_layerOrder.end() || it + 1 == m_layerOrder.end()) {
    return false;  // Not found or already at bottom
  }

  // Swap with next element
  std::iter_swap(it, it + 1);
  UpdateCache();
  return true;
}

std::vector<wxString> GRIBLayerSet::GetLayerOrder() const {
  return m_layerOrder;
}

void GRIBLayerSet::UpdateLayerOrder() {
  // Remove layers that no longer exist.
  m_layerOrder.erase(std::remove_if(m_layerOrder.begin(), m_layerOrder.end(),
                                    [this](const wxString& name) {
                                      return m_layers.find(name) ==
                                             m_layers.end();
                                    }),
                     m_layerOrder.end());

  // Add new layers that aren't in the order list
  for (const auto& pair : m_layers) {
    if (std::find(m_layerOrder.begin(), m_layerOrder.end(), pair.first) ==
        m_layerOrder.end()) {
      m_layerOrder.push_back(pair.first);
    }
  }
}

std::vector<GRIBLayer*> GRIBLayerSet::GetEnabledLayers() const {
  std::vector<GRIBLayer*> enabled;
  for (auto& pair : m_layers) {
    if (pair.second && pair.second->IsEnabled()) {
      enabled.push_back(pair.second.get());
    }
  }
  return enabled;
}

std::vector<wxDateTime> GRIBLayerSet::GetForecastTimes() const {
  std::set<wxDateTime, std::less<wxDateTime>> uniqueTimes;
  for (const auto& layer : GetEnabledLayers()) {
    if (!layer->IsEnabled()) continue;

    const ArrayOfGribRecordSets* rsa = layer->GetFile()->GetRecordSetArrayPtr();
    for (size_t i = 0; i < rsa->GetCount(); i++) {
      wxDateTime t(rsa->Item(i).GetReferenceTime());
      uniqueTimes.insert(t);
    }
  }
  return std::vector<wxDateTime>(uniqueTimes.begin(), uniqueTimes.end());
}

// Get total time span in hours
int GRIBLayerSet::GetForecastTimeSpan() const {
  auto timestamps = GetForecastTimes();
  if (timestamps.size() < 2) return 0;

  return wxTimeSpan(timestamps.back() - timestamps.front()).GetHours();
}

wxDateTime GRIBLayerSet::MinForecastTime() const {
  auto timestamps = GetForecastTimes();
  return timestamps.empty() ? wxDateTime::Now() : timestamps.front();
}

int GRIBLayerSet::GetSmallestInterval() const {
  auto timestamps = GetForecastTimes();
  if (timestamps.size() < 2) return 0;

  int minInterval = INT_MAX;
  for (size_t i = 1; i < timestamps.size(); i++) {
    wxTimeSpan interval = timestamps[i] - timestamps[i - 1];
    minInterval = wxMin(minInterval, interval.GetMinutes());
  }
  return minInterval;
}

double GRIBLayerSet::getTimeInterpolatedValue(int idx, double lon, double lat,
                                              wxDateTime time) const {
  if (!IsOK()) return GRIB_NOTDEF;
  const ArrayOfGribRecordSets* rsa = GetRecordSetArrayPtr();

  if (rsa->GetCount() == 0) return GRIB_NOTDEF;

  GribRecord *before = nullptr, *after = nullptr;

  unsigned int j;
  time_t t = time.GetTicks();
  for (j = 0; j < rsa->GetCount(); j++) {
    GribRecordSet* GRS = &rsa->Item(j);
    GribRecord* GR = GRS->GetRecord(idx);
    if (!GR) continue;

    time_t curtime = GR->getRecordCurrentDate();
    if (curtime == t) return GR->getInterpolatedValue(lon, lat);

    if (curtime < t) before = GR;

    if (curtime > t) {
      after = GR;
      break;
    }
  }
  // time_t wxDateTime::GetTicks();
  if (!before || !after) return GRIB_NOTDEF;

  time_t t1 = before->getRecordCurrentDate();
  time_t t2 = after->getRecordCurrentDate();
  if (t1 == t2) return before->getInterpolatedValue(lon, lat);

  double v1 = before->getInterpolatedValue(lon, lat);
  double v2 = after->getInterpolatedValue(lon, lat);
  if (v1 != GRIB_NOTDEF && v2 != GRIB_NOTDEF) {
    double k = fabs((double)(t - t1) / (t2 - t1));
    return (1.0 - k) * v1 + k * v2;
  }

  return GRIB_NOTDEF;
}

bool GRIBLayerSet::getTimeInterpolatedValues(double& M, double& A, int idx1,
                                             int idx2, double lon, double lat,
                                             wxDateTime time) const {
  M = GRIB_NOTDEF;
  A = GRIB_NOTDEF;

  if (!IsOK()) return false;
  const ArrayOfGribRecordSets* rsa = GetRecordSetArrayPtr();

  if (rsa->GetCount() == 0) return false;

  GribRecord *beforeX = nullptr, *afterX = nullptr;
  GribRecord *beforeY = nullptr, *afterY = nullptr;

  unsigned int j;
  time_t t = time.GetTicks();
  for (j = 0; j < rsa->GetCount(); j++) {
    GribRecordSet* GRS = &rsa->Item(j);
    GribRecord* GX = GRS->GetRecord(idx1);
    GribRecord* GY = GRS->GetRecord(idx2);
    if (!GX || !GY) continue;

    time_t curtime = GX->getRecordCurrentDate();
    if (curtime == t) {
      return GribRecord::getInterpolatedValues(M, A, GX, GY, lon, lat, true);
    }
    if (curtime < t) {
      beforeX = GX;
      beforeY = GY;
    }
    if (curtime > t) {
      afterX = GX;
      afterY = GY;
      break;
    }
  }
  // time_t wxDateTime::GetTicks();
  if (!beforeX || !afterX) return false;

  time_t t1 = beforeX->getRecordCurrentDate();
  time_t t2 = afterX->getRecordCurrentDate();
  if (t1 == t2) {
    return GribRecord::getInterpolatedValues(M, A, beforeX, beforeY, lon, lat,
                                             true);
  }
  double v1m, v2m, v1a, v2a;
  if (!GribRecord::getInterpolatedValues(v1m, v1a, beforeX, beforeY, lon, lat,
                                         true))
    return false;

  if (!GribRecord::getInterpolatedValues(v2m, v2a, afterX, afterY, lon, lat,
                                         true))
    return false;

  if (v1m == GRIB_NOTDEF || v2m == GRIB_NOTDEF || v1a == GRIB_NOTDEF ||
      v2a == GRIB_NOTDEF)
    return false;

  double k = fabs((double)(t - t1) / (t2 - t1));
  M = (1.0 - k) * v1m + k * v2m;
  A = (1.0 - k) * v1a + k * v2a;
  return true;
}

/** @todo refactor and remove. */
GribTimelineRecordSet* GRIBLayerSet::GetTimeLineRecordSet(
    const wxDateTime* time) {
  if (time == nullptr) {
    time = &m_selectedTime;
  }
  for (auto& pair : m_layers) {
    if (pair.second && pair.second->IsEnabled()) {
      // Return the first valid timeline record set for backwards compatibility.
      wxLogMessage(
          "GRIBLayerSet::GetTimeLineRecordSet: Found enabled layer: %s",
          pair.first);
      return pair.second->GetTimeLineRecordSet(*time);
    }
  }
  wxLogMessage("GRIBLayerSet::GetTimeLineRecordSet: No enabled layers found.");
  return nullptr;
}

GribRecord* GRIBLayerSet::CreateInterpolateGRIBdRecord(
    int recordIndex, double latMin, double latMax, double lonMin, double lonMax,
    int targetPoints, const wxDateTime* time = nullptr) const {
  // Calculate grid spacing to achieve approximately targetPoints
  double latRange = latMax - latMin;
  double lonRange = lonMax - lonMin;
  double aspectRatio = lonRange / latRange;

  // Solve: ni * nj ≈ targetPoints where ni/nj = aspectRatio
  // ni = aspectRatio * nj
  // nj * (aspectRatio * nj) = targetPoints
  // nj² * aspectRatio = targetPoints
  int nj = sqrt(targetPoints / aspectRatio);
  int ni = targetPoints / nj;  // Rounds down, which is fine

  double gridSpacing = latRange / nj;  // Use latitude range for base spacing

  std::unique_ptr<GribRecord> record(new GribRecord());
  // @todo initialize record with grid spacing, lonMin, latMin, ni, nj
  // record->setDiHeader(gridSpacing, lonMin, latMin, ni, nj);

  // Fill the grid with interpolated values
  for (int i = 0; i < ni; i++) {
    for (int j = 0; j < nj; j++) {
      double lon = lonMin + i * gridSpacing;
      double lat = latMin + j * gridSpacing;

      double value = GetInterpolatedValue(recordIndex, lon, lat, time, true);

      record->setValue(i, j,
                       value);  // Will be GRIB_NOTDEF where no data exists
    }
  }

  return record->isOk() ? record.release() : nullptr;
}

bool GRIBLayerSet::GetZoneLimits(double* latmin, double* latmax, double* lonmin,
                                 double* lonmax) const {
  double ltmi = GRIB_NOTDEF;
  double ltma = -GRIB_NOTDEF;
  double lnmi = GRIB_NOTDEF;
  double lnma = -GRIB_NOTDEF;
  bool found = false;
  bool crosses_dateline = false;

  // Iterate through enabled layers
  for (const auto& layer : GetEnabledLayers()) {
    const GRIBFile* file = layer->GetFile();
    if (!file) continue;

    // Get record sets from this layer
    const ArrayOfGribRecordSets* recordSets = file->GetRecordSetArrayPtr();
    if (!recordSets) continue;

    // Check each record set (time point)
    for (size_t i = 0; i < recordSets->GetCount(); i++) {
      const GribRecordSet& recordSet = recordSets->Item(i);

      // Check all records in the set
      for (int j = 0; j < Idx_COUNT; j++) {
        const GribRecord* record = recordSet.GetRecord(j);
        if (!record) continue;

        // Clamp latitude to valid range
        double rec_latmin = wxMax(-90.0, record->getLatMin());
        double rec_latmax = wxMin(90.0, record->getLatMax());

        // Handle longitude normalization
        double rec_lonmin = record->getLonMin();
        double rec_lonmax = record->getLonMax();

        // Normalize longitudes to -180 to 180 range
        while (rec_lonmin <= -180.0) rec_lonmin += 360.0;
        while (rec_lonmin > 180.0) rec_lonmin -= 360.0;
        while (rec_lonmax <= -180.0) rec_lonmax += 360.0;
        while (rec_lonmax > 180.0) rec_lonmax -= 360.0;

        // Check if this record crosses the dateline
        if (rec_lonmax < rec_lonmin) {
          crosses_dateline = true;
          lnmi = wxMin(lnmi, -180.0);
          lnma = wxMax(lnma, 180.0);
        } else {
          lnmi = wxMin(lnmi, rec_lonmin);
          lnma = wxMax(lnma, rec_lonmax);
        }

        ltmi = wxMin(ltmi, rec_latmin);
        ltma = wxMax(ltma, rec_latmax);
        found = true;
      }
    }
  }

  // Check if we found any valid records
  if (!found || ltmi == GRIB_NOTDEF || ltma == -GRIB_NOTDEF) {
    return false;
  }

  // Handle the case where non-crossing records span most of the globe
  if (!crosses_dateline && (lnma - lnmi) > 350.0) {
    lnmi = -180.0;
    lnma = 180.0;
  }

  if (latmin) *latmin = ltmi;
  if (latmax) *latmax = ltma;
  if (lonmin) *lonmin = lnmi;
  if (lonmax) *lonmax = lnma;

  return true;
}

// @todo: need lat/long bounding box to reduce the search space to what is
// visible on the screen.
bool GRIBLayerSet::GetRandomValidCoordinate(int idx, int idy, float& outLon,
                                            float& outLat,
                                            int maxAttempts) const {
  if (!IsOK() || idx < 0 || idx >= Idx_COUNT || idy < -1 || idy >= Idx_COUNT) {
    return false;
  }

  // Get enabled layers
  std::vector<GRIBLayer*> enabled = GetEnabledLayers();
  if (enabled.empty()) {
    return false;
  }

  // Filter layers to only those that have our required data
  std::vector<std::pair<GRIBLayer*, GribRecord*>> validLayers;
  validLayers.reserve(enabled.size());

  for (GRIBLayer* layer : enabled) {
    GribTimelineRecordSet* records =
        layer->GetTimeLineRecordSet(m_selectedTime);
    if (!records) continue;

    // For vector quantities, need both components
    if (idy >= 0) {
      GribRecord* grX = records->GetRecord(idx);
      GribRecord* grY = records->GetRecord(idy);
      if (grX && grY) {
        validLayers.emplace_back(layer, grX);
      }
    } else {
      // For scalar quantities, just need the one record
      GribRecord* gr = records->GetRecord(idx);
      if (gr) {
        validLayers.emplace_back(layer, gr);
      }
    }
  }

  if (validLayers.empty()) {
    return false;
  }

  // Try to find valid coordinates, using a smarter selection strategy
  for (int attempt = 0; attempt < maxAttempts; attempt++) {
    // Randomly select one of the valid layers to try
    size_t layerIdx = rand() % validLayers.size();
    auto& layerPair = validLayers[layerIdx];
    GribRecord* record = layerPair.second;

    // Get bounds for this specific record
    double minLon = record->getLonMin();
    double maxLon = record->getLonMax();
    double minLat = record->getLatMin();
    double maxLat = record->getLatMax();

    // Generate random position within this layer's bounds
    float lon = (float)rand() / RAND_MAX * (maxLon - minLon) + minLon;
    float lat = (float)rand() / RAND_MAX * (maxLat - minLat) + minLat;

    // For vector quantities
    if (idy >= 0) {
      double magnitude, angle;
      if (GetInterpolatedVector(magnitude, angle, idx, idy, lon, lat)) {
        if (magnitude > 0 && magnitude < 100) {
          // For current, try to find areas with stronger flow
          // For currents, progressively relax minimum speed requirement
          double minSpeed = (idx == Idx_SEACURRENT_VX)
                                ? 1 - (double)attempt / maxAttempts
                                : 0;
          if (magnitude > minSpeed) {
            outLon = lon;
            outLat = lat;
            return true;
          }
        }
      }
    } else {
      // For scalar quantities
      double value = GetInterpolatedValue(idx, lon, lat);
      if (value != GRIB_NOTDEF) {
        outLon = lon;
        outLat = lat;
        return true;
      }
    }
  }
  return false;
}

double GRIBLayerSet::GetCoverageArea(int idx) const {
  if (!IsOK() || idx < 0 || idx >= Idx_COUNT) {
    return 0.0;
  }

  // Get enabled layers that have our data
  std::vector<const GribRecord*> records;
  for (GRIBLayer* layer : GetEnabledLayers()) {
    GribTimelineRecordSet* timelineSet =
        layer->GetTimeLineRecordSet(m_selectedTime);
    if (!timelineSet) continue;

    const GribRecord* record = timelineSet->GetRecord(idx);
    if (record) {
      records.push_back(record);
    }
  }

  if (records.empty()) {
    return 0.0;
  }

  // For a single record, just return its area
  if (records.size() == 1) {
    return records[0]->GetGribArea();
  }

  // For multiple records, use inclusion-exclusion principle
  double totalArea = 0;

  // Add all individual areas
  for (const GribRecord* record : records) {
    totalArea += record->GetGribArea();
  }

  // Subtract intersections
  for (size_t i = 0; i < records.size(); i++) {
    for (size_t j = i + 1; j < records.size(); j++) {
      totalArea -= records[i]->GetIntersectionArea(records[j]);
    }
  }

  return totalArea;
}