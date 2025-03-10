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

#ifndef __GRIB_LAYER_H__
#define __GRIB_LAYER_H__

#include <memory>
#include <wx/string.h>

#include "ocpn_plugin.h"

#include "GribReader.h"
#include "grib_file.h"

class GribTimelineRecordSet;
class GRIBLayerSet;

/**
 * Represents a single GRIB data layer that can be enabled/disabled
 * independently.
 *
 * The GRIBLayer class provides a wrapper around GRIBFile that allows individual
 * control over weather data access and usage. Each layer can be independently
 * enabled or disabled, facilitating:
 * - Comparison of different weather models
 * - Selective use of weather data for calculations (e.g., weather routing)
 * - Choice of different data sources for visualization and computations
 * - Organization of GRIB data by source or time period
 *
 * When a layer is disabled, its data is excluded from:
 * - Visualization on the chart.
 * - Weather routing calculations.
 * - Data interpolation requests.
 * - Timeline record sets.
 */
class DECL_EXP GRIBLayer {
public:
  /**
   * Constructs a new GRIB layer.
   *
   * @param name User-friendly identifier for the layer
   * @param file Pointer to GRIB file to manage (takes ownership)
   * @param parent Pointer to parent layer set (non-owning)
   */
  GRIBLayer(const wxString& name, GRIBFile* file, GRIBLayerSet* parent);
  ~GRIBLayer();

  /**
   * Checks if layer is currently enabled for display.
   *
   * @return true if layer is enabled, false otherwise
   */
  bool IsEnabled() const { return m_enabled; }
  /**
   * Sets layer enable/disable state.
   * @param enabled New state for the layer
   */
  void SetEnabled(bool enabled);
  /**
   * Checks if layer has valid GRIB data.
   * @return true if layer has a valid GRIB file, false otherwise
   */
  bool IsOK() const;

  /**
   * Gets layer identifier.
   *
   * @return Const reference to layer name
   */
  const wxString& GetName() const { return m_name; }
  /**
   * Gets last error message if any
   * @return Error message string
   */
  wxString GetLastError() const;
  /**
   * Gets reference date/time of GRIB data.
   *
   * @return Unix timestamp of reference time
   */
  time_t GetRefDateTime() const;

  /**
   * Gets modifiable access to underlying GRIB file.
   *
   * @return Pointer to GRIB file
   */
  GRIBFile* GetFile() { return m_file.get(); }
  /**
   * Gets read-only access to underlying GRIB file.
   *
   * @return Const pointer to GRIB file
   */
  const GRIBFile* GetFile() const { return m_file.get(); }

  /**
   * Replaces current GRIB file with a new one.
   *
   * @param file Pointer to new GRIB file (takes ownership)
   */
  void SetGribFile(GRIBFile* file);

  unsigned int GetId() const { return m_id; }

  /**
   * Get the minimum time available in the layer.
   */
  wxDateTime MinTime();

  /**
   * Retrieve or create a temporally interpolated GRIB record set for a
   * specific timestamp.
   *
   * This function performs temporal interpolation between two known timestamps
   * to estimate values at the requested time. The actual data values at each
   * lat/lon point are either:
   * 1. Linearly interpolated in time for scalar values
   * 2. Vector interpolated (2D) for wind and current vectors to maintain
   * physical consistency
   *
   * @note This function does NOT perform spatial (lat/lon) interpolation.
   *
   * @note Timestamp handling:
   *       - For timestamps between forecast times (e.g. 1AM between 12AM and
   * 3AM forecasts), data is interpolated between the bracketing forecasts.
   *       - For timestamps exactly matching forecast times (e.g. 12AM, 3AM),
   *         the original GRIB record is used directly without interpolation to
   * avoid unnecessary computation and maintain precision.
   *
   * @param time The target datetime for which to interpolate GRIB records.
   * @return Pointer to GribTimelineRecordSet containing temporally interpolated
   * data, or NULL if no valid data.
   */
  GribTimelineRecordSet* GetTimeLineRecordSet(wxDateTime time);

private:
  wxString m_name;                   //!< User-friendly layer name
  bool m_enabled{true};              //!< Layer visibility state
  std::unique_ptr<GRIBFile> m_file;  //!< Underlying GRIB file data
  GRIBLayerSet* m_parent{nullptr};  //!< Non-owning pointer to parent layer set.
  mutable unsigned int m_id{0};

  /**
   * Cached temporally interpolated records for the currently selected time.
   *
   * GribTimelineRecordSet contains:
   * - Records for all weather parameters (wind, pressure, etc)
   * - Values interpolated between available time points in GRIB file
   * - Cached isobar calculations for rendering
   *
   * The cache is:
   * - Created on first request for a time point
   * - Updated when a different time is requested
   * - Shared by all interpolation requests at the same time point
   * - Used to avoid repeatedly calculating temporal interpolation
   *
   * @note Memory is owned by GRIBLayer and freed in destructor
   * @see m_timelineTime for the time point this cache represents
   */
  GribTimelineRecordSet* m_timelineRecordSet{nullptr};
  /** Time point for which we cached the record set. */
  wxDateTime m_timelineTime;
};

#endif  // __GRIB_LAYER_H__