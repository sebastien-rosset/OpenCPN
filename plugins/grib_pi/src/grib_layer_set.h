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

#ifndef __GRIB_LAYER_SET_H__
#define __GRIB_LAYER_SET_H__

#include <map>
#include <memory>
#include <vector>
#include <wx/string.h>
#include <wx/dynarray.h>

#include "ocpn_plugin.h"

#include "GribRecord.h"
#include "GribRecordSet.h"
#include "grib_file.h"
#include "grib_layer.h"
#include "grib_layer_set.h"
#include "grib_timeline_record_set.h"
#include "grib_layer_merge_strategy.h"

/**
 * Manages a collection of GRIB layers and their interactions.
 *
 * GRIBLayerSet coordinates multiple GRIB layers, handling:
 * - Layer addition, removal, and reordering.
 * - Layer enable/disable state.
 * - Data interpolation across enabled layers.
 */
class DECL_EXP GRIBLayerSet {
public:
  GRIBLayerSet() = default;
  ~GRIBLayerSet() = default;

  // Delete copy operations since we have a unique_ptr member.
  GRIBLayerSet(const GRIBLayerSet&) = delete;
  GRIBLayerSet& operator=(const GRIBLayerSet&) = delete;

  // Allow move operations
  GRIBLayerSet(GRIBLayerSet&&) = default;
  GRIBLayerSet& operator=(GRIBLayerSet&&) = default;

  /** Return true if at least one layer is enabled and OK. */
  bool IsOK() const;
  /**
   * Get the list of source filenames being used across all layers.
   */
  const wxArrayString& GetFileNames() const;
  /** Get the last error message if file loading failed. */
  wxString GetLastMessage() const;
  /**
   * Gets pointer to array of GribRecordSets from the first enabled GRIB layer.
   *
   * @deprecated This function has multiple issues and should not be used:
   *   - Only returns data from the first enabled layer, ignoring others.
   *   - Caller has no way to know which layer's data they received.
   *
   * @return Pointer to array of GribRecordSets from first enabled layer, or
   *         nullptr if no layers are enabled.
   */
  const ArrayOfGribRecordSets* GetRecordSetArrayPtr() const;
  /**
   * Get reference datetime of the GRIB data (earliest time).
   */
  time_t GetRefDateTime() const;

  /**
   * Set the currently selected forecast time.
   *
   * This time represents the forecast time point selected by the user in the
   * timeline, which is used as the default time for interpolation requests
   * when no specific time is provided.
   *
   * @param time The forecast time to set as currently selected.
   */
  // @todo Make sure this function is called when the timeline selection
  // changes.
  void SetSelectedTime(wxDateTime time) { m_selectedTime = time; }

  /**
   * Get the currently selected forecast time.
   *
   * @return The forecast time currently selected in the timeline.
   */
  wxDateTime GetSelectedTime() const { return m_selectedTime; }

  /**
   * Get array of available data types in this file.
   *
   * A structured array of integers, each representing a unique GRIB parameter
   * and its vertical level information. This array is used to track what
   * meteorological parameters are available in a GRIB file or layer.
   *
   * The array is a union of all available data types across all layers.
   */
  GRIBFile::GribIdxArray& GetGribIdxArray();

  /**
   * Add a new layer to the set.
   *
   * @param name Unique identifier for the layer
   * @param file GRIB file to include in the layer
   * @return Pointer to the created layer or nullptr if creation failed.
   */
  GRIBLayer* AddLayer(const wxString& name, const wxString& file,
                      bool copyFirstCumRec, bool copyMissWaveRec,
                      bool newestFile);

  /**
   * Remove a layer from the set.
   *
   * @param name Layer identifier
   * @return true if layer was found and removed.
   */
  bool RemoveLayer(const wxString& name);

  void ClearLayers();

  /**
   * Find a layer by name.
   *
   * @param name Layer identifier
   * @return Pointer to the layer or nullptr if not found.
   */
  GRIBLayer* GetLayer(const wxString& name);
  /**
   * Find a layer by name (const version).
   *
   * @param name Layer identifier
   * @return Pointer to the layer or nullptr if not found.
   */
  const GRIBLayer* GetLayer(const wxString& name) const;

  /**
   * Move a layer up in the rendering order.
   *
   * @param name The name of the layer to move
   * @return true if the layer was moved, false if at top or not found
   */
  bool MoveLayerUp(const wxString& name);

  /**
   * Move a layer down in the rendering order.
   *
   * @param name The name of the layer to move
   * @return true if the layer was moved, false if at bottom or not found
   */
  bool MoveLayerDown(const wxString& name);

  /**
   * Get the current layer order.
   *
   * @return Vector of layer names in rendering order (top to bottom)
   */
  std::vector<wxString> GetLayerOrder() const;

  /**
   * Get all currently enabled layers.
   *
   * @return Vector of pointers to enabled layers
   */
  std::vector<GRIBLayer*> GetEnabledLayers() const;

  /**
   * Update all caches.
   * This should be called after any changes to the layer set.
   */
  void UpdateCache();

  /**
   * Configure how records from multiple layers should be merged.
   * @param method The scoring approach to use
   */
  void SetMergeStrategy(LayerMergeStrategy::ScoringMethod method) {
    m_mergeStrategy.SetScoringMethod(method);
    UpdateCache();
  }

  /**
   * Get current merge strategy settings.
   * @return Reference to merge strategy object
   */
  const LayerMergeStrategy& GetMergeStrategy() const { return m_mergeStrategy; }

  /**
   * Get spatially interpolated scalar value at exact lat/lon position, for a
   * specific time, across all enabled layers.
   *
   * Use the merge strategy to select or combine records for scalar quantities
   * like temperature, pressure, etc.
   *
   * @param recordIndex Index identifying the weather parameter to interpolate.
   * @param lon Longitude in degrees.
   * @param lat Latitude in degrees.
   * @param time Target time for temporal interpolation. If nullptr, uses the
   * currently selected forecast time from the timeline.
   * @param numericalInterpolation
   *   - When false, uses nearest neighbor interpolation by selecting closest
   * grid point
   *   - When true, performs bilinear interpolation between surrounding grid
   * points.
   * @return Spatially interpolated value or GRIB_NOTDEF if point is outside all
   * layer boundaries or insufficient valid grid points exist for interpolation.
   */
  double GetInterpolatedValue(int recordIndex, double lon, double lat,
                              const wxDateTime* time = nullptr,
                              bool numericalInterpolation = true) const;

  /**
   * Get spatially interpolated vector values at exact lat/lon position, for a
   * specific time, across all enabled layers.
   *
   * Use the merge strategy to select or combine records for vector quantities
   * like wind or current, preserving proper vector mathematics during
   * interpolation.
   *
   * @param magnitude [out] Vector magnitude at interpolated point (preserves
   * input units).
   * @param angle [out] Vector direction in meteorological degrees (0=N, 90=E)
   * at interpolated point.
   * @param xComponentIndex Index for the X/U vector component record.
   * @param yComponentIndex Index for the Y/V vector component record.
   *
   * @param px Longitude in degrees.
   * @param py Latitude in degrees.
   * @param time Target time for temporal interpolation. If nullptr, uses the
   * currently selected forecast time from the timeline.
   * @param numericalInterpolation
   *   - When false, uses nearest neighbor interpolation by selecting closest
   *     grid point.
   *   - When true, performs bilinear interpolation between surrounding grid
   * points.
   *
   * @return true if interpolation successful, false if point is outside all
   * layer boundaries or insufficient valid grid points exist for interpolation.
   *
   * @note Vector components must follow meteorological conventions where:
   *       - U (xComponent) is positive eastward
   *       - V (yComponent) is positive northward
   */
  bool GetInterpolatedVector(double& magnitude, double& angle,
                             int xComponentIndex, int yComponentIndex,
                             double lon, double lat,
                             const wxDateTime* time = nullptr,
                             bool numericalInterpolation = true) const;

  /**
   * Returns all unique forecast valid times across enabled GRIB layers.
   *
   * Iterates through all enabled GRIB layers, collecting the reference times
   * from each layer's record sets. Each valid time represents when the forecast
   * data is actually valid, i.e.: valid_time = model_init_time +
   * forecast_period
   *
   * @return Vector of unique valid times ordered chronologically.
   */
  std::vector<wxDateTime> GetForecastTimes() const;

  /**
   * Returns the earliest forecast valid time across all enabled layers.
   */
  wxDateTime MinForecastTime() const;

  /**
   * Returns the total duration of the forecast in hours.
   *
   * Calculates the time difference between the earliest and latest forecast
   * valid times. For example, if forecasts range from 2024-02-11 00:00Z to
   * 2024-02-12 00:00Z, this returns 24 (hours).
   *
   * @note Care should be taken when interpreting the result as this function
   * simply returns the raw time difference between the earliest and latest
   * times present. It does not validate whether these times are from the same
   * forecast model run or are meteorologically meaningful. For example, if
   * timestamps come from different GRIB files or model runs (like one from 2023
   * and another from 2024), the result could be unexpectedly large and not
   * represent a valid forecast duration.
   *
   * @return Number of hours between first and last forecast valid time.
   *         Returns 0 if there are fewer than 2 forecast times.
   */
  int GetForecastTimeSpan() const;

  /**
   * Returns the smallest time interval between consecutive forecast valid times
   * in minutes.
   *
   * Examines all pairs of consecutive forecast times to find the minimum
   * interval. For example, with forecasts at [00Z, 03Z, 06Z, 07Z, 12Z], this
   * would return 60 (minutes) from the 06Z-07Z interval.
   *
   * @note Like GetForecastDurationHours(), this function performs raw time
   * difference calculations without validating whether the times come from the
   * same forecast model run. Mixing forecasts from different sources or model
   * runs could result in intervals that don't reflect the actual forecast time
   * step of any single model.
   *
   * @return Minimum interval in minutes between consecutive forecast times.
   *         Returns 0 if there are fewer than 2 forecast times.
   */
  int GetSmallestInterval() const;

  /**
   * Get the weather parameter values at a specific time.
   *
   * @param time Optional time for temporal interpolation. If nullptr, uses
   * current selected time.
   * @return Pointer to GribTimelineRecordSet containing temporally interpolated
   * data, or NULL if no valid data.
   *
   * @note Return the values from the first layer that has data.
   * @todo Refactor to use the merge strategy.
   */
  GribTimelineRecordSet* GetTimeLineRecordSet(const wxDateTime* time = nullptr);

  /**
   * Gets the geographic bounds encompassing all enabled GRIB layers.
   *
   * Calculates the minimum/maximum latitude and longitude values that contain
   * all data across enabled layers and all weather parameters. This defines
   * the total area where GRIB data is available.
   *
   * @param[out] latmin Minimum latitude if not null
   * @param[out] latmax Maximum latitude if not null
   * @param[out] lonmin Minimum longitude if not null
   * @param[out] lonmax Maximum longitude if not null
   * @return true if valid bounds were found, false if no enabled layers or no
   * valid records
   */
  bool GetZoneLimits(double* latmin, double* latmax, double* lonmin,
                     double* lonmax) const;

  /**
   * Calculate the total geographic area covered by valid data for a specific
   * parameter.
   *
   * For areas where multiple layers overlap, the area is counted only once.
   * The area calculation takes into account the Earth's spherical nature by
   * using the cosine of latitude.
   *
   * @param idx Index identifying which record to access (see Idx_* enum)
   * @return Total area in square degrees where valid data exists
   */
  double GetCoverageArea(int idx) const;

  /**
   * Get random coordinates within GRIB layer bounds for a specific data index.
   *
   * Generates random coordinates that fall within the bounds of enabled layers
   * and contain valid data for the requested record index. For vector
   * quantities like wind or current, pass the X component index - the function
   * will automatically check for corresponding Y component.
   *
   * @param idx Index identifying which record to access (see Idx_* enum)
   * @param idy Index for the Y component of a vector quantity (e.g., wind)
   * @param outLon Output longitude value if valid point found
   * @param outLat Output latitude value if valid point found
   * @param maxAttempts Maximum attempts to find valid coordinates (default 20)
   * @return true if valid coordinates found, false otherwise
   */
  bool GetRandomValidCoordinate(int idx, int idy, float& outLon, float& outLat,
                                int maxAttempts = 20) const;

  double getTimeInterpolatedValue(int idx, double lon, double lat,
                                  wxDateTime t) const;
  bool getTimeInterpolatedValues(double& M, double& A, int idx1, int idx2,
                                 double lon, double lat, wxDateTime t) const;

  /**
   * Creates a GribRecord by interpolating values across all enabled layers in a
   * lat/lon region.
   *
   * Uses the configured layer merge strategy to select or combine data when
   * multiple layers contain values for the requested parameter. The resulting
   * GribRecord represents a regular grid of values interpolated both spatially
   * within each layer and across layers when overlapping data exists.
   *
   * For points that fall outside all enabled layers, GRIB_NOTDEF will be set.
   * This is normal when dealing with multiple GRIB files that only partially
   * cover the region. The caller (e.g., isobar rendering) must handle areas
   * with no data appropriately.
   *
   * @param recordIndex Index identifying the weather parameter to interpolate
   * @param latMin Minimum latitude in degrees
   * @param latMax Maximum latitude in degrees
   * @param lonMin Minimum longitude in degrees
   * @param lonMax Maximum longitude in degrees
   * @param targetPoints Desired number of grid points (actual may vary
   * slightly)
   * @param time Optional time for temporal interpolation. If nullptr, uses
   * current selected time.
   * @return New GribRecord containing interpolated values with GRIB_NOTDEF
   * where no data exists. Returns nullptr only on error. Caller takes
   * ownership.
   */
  GribRecord* CreateInterpolateGRIBdRecord(int recordIndex, double latMin,
                                           double latMax, double lonMin,
                                           double lonMax, int targetPoints,
                                           const wxDateTime* time) const;

private:
  /** Update layer order after adding/removing layers. */
  void UpdateLayerOrder();
  /** Update the cache of GRIB indices. */
  void UpdateIdxArrayCache();
  /** Update the cache of filenames. */
  void UpdateFileNameCache();

  /**
   * Find the best matching GRIB record for a given parameter across enabled
   * layers.
   *
   * Uses the merge strategy to select the most appropriate record when multiple
   * layers contain data for the requested parameter type.
   *
   * @param recordIndex Index identifying the weather parameter to find.
   * @param time Target time for record selection. If nullptr, uses the
   * currently selected forecast time from the timeline.
   * @return Pointer to the best matching GribRecord, or nullptr if no valid
   * record found in enabled layers.
   */
  GribRecord* findBestRecord(int recordIndex,
                             const wxDateTime* time = nullptr) const;

  /**
   * Find the best matching pair of vector component records across enabled
   * layers.
   *
   * Uses the merge strategy to select the most appropriate vector records (U/V
   * or X/Y components) when multiple layers contain vector data. The returned
   * records will always be from the same layer to maintain physical
   * consistency.
   *
   * @param xComponentIndex Index for the X/U vector component record.
   * @param yComponentIndex Index for the Y/V vector component record.
   * @param lon Longitude in degrees (used in merge strategy scoring).
   * @param lat Latitude in degrees (used in merge strategy scoring).
   * @param time Target time for record selection. If nullptr, uses the
   * currently selected forecast time from the timeline.
   * @return Pair of pointers to the best matching X and Y component records.
   *         Either or both pointers may be nullptr if no valid records found.
   */
  std::pair<GribRecord*, GribRecord*> findBestRecordVector(
      int xComponentIndex, int yComponentIndex, double lon, double lat,
      const wxDateTime* time = nullptr) const;

  wxString m_LastMessage;

  /**
   * Collection of GRIB layers indexed by their unique name.
   * Each layer is associated with a single GRIB file that may contain multiple
   * meteorological parameters.
   * Multiple layers can be enabled simultaneously, allowing for data
   * visualization and interpolation across different GRIB files.
   */
  std::map<wxString, std::unique_ptr<GRIBLayer>> m_layers;

  /**
   * Ordered list of layer identifiers.
   */
  std::vector<wxString> m_layerOrder;

  /**
   * Array of indices indicating available data types.
   *
   * A structured array of integers, each representing a unique GRIB parameter
   * and its vertical level information. This array is used to track what
   * meteorological parameters are available in a GRIB file or layer.
   */
  GRIBFile::GribIdxArray m_gribIdxArray;

  /** For collecting filenames across layers. */
  wxArrayString m_fileNames;

  ArrayOfGribRecordSets
      m_emptyRecordSetArray;  //!< For when no layers are enabled.

  /** Strategy for merging records from multiple layers. */
  LayerMergeStrategy m_mergeStrategy;
  /** Currently selected forecast time from timeline. */
  wxDateTime m_selectedTime;
};

#endif  // __GRIB_LAYER_SET_H__