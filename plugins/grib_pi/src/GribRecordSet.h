/***************************************************************************
 *   Copyright (C) 2010 by David S. Register                               *
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
/**
 * \file
 * GRIB Record Set Management.
 *
 * Defines classes for managing collections of GRIB records that share
 * the same reference time. A record set combines multiple meteorological
 * (wind, pressure, waves, etc.) valid at a single forecast time.
 */
#ifndef GRIB_RECORD_SET_H
#define GRIB_RECORD_SET_H

#include "GribRecord.h"

// These are indexes into the array
enum {
  Idx_WIND_VX,        //!< Surface wind velocity X component
  Idx_WIND_VX850,     //!< Wind velocity X component at 850 hPa
  Idx_WIND_VX700,     //!< Wind velocity X component at 700 hPa
  Idx_WIND_VX500,     //!< Wind velocity X component at 500 hPa
  Idx_WIND_VX300,     //!< Wind velocity X component at 300 hPa
  Idx_WIND_VY,        //!< Surface wind velocity Y component
  Idx_WIND_VY850,     //!< Wind velocity Y component at 850 hPa
  Idx_WIND_VY700,     //!< Wind velocity Y component at 700 hPa
  Idx_WIND_VY500,     //!< Wind velocity Y component at 500 hPa
  Idx_WIND_VY300,     //!< Wind velocity Y component at 300 hPa
  Idx_WIND_GUST,      //!< Wind gust speed at surface
  Idx_PRESSURE,       //!< Surface pressure
  Idx_HTSIGW,         //!< Significant wave height
  Idx_WVDIR,          //!< Wave direction
  Idx_WVPER,          //!< Wave period
  Idx_SEACURRENT_VX,  //!< Sea current velocity X component
  Idx_SEACURRENT_VY,  //!< Sea current velocity Y component
  Idx_PRECIP_TOT,     //!< Total precipitation
  Idx_CLOUD_TOT,      //!< Total cloud cover
  Idx_AIR_TEMP,       //!< Air temperature at 2m
  Idx_AIR_TEMP850,    //!< Air temperature at 850 hPa
  Idx_AIR_TEMP700,    //!< Air temperature at 700 hPa
  Idx_AIR_TEMP500,    //!< Air temperature at 500 hPa
  Idx_AIR_TEMP300,    //!< Air temperature at 300 hPa
  Idx_SEA_TEMP,       //!< Sea surface temperature
  Idx_CAPE,           //!< Convective Available Potential Energy
  Idx_COMP_REFL,      //!< Composite radar reflectivity
  Idx_HUMID_RE,       //!< Surface relative humidity
  Idx_HUMID_RE850,    //!< Relative humidity at 850 hPa
  Idx_HUMID_RE700,    //!< Relative humidity at 700 hPa
  Idx_HUMID_RE500,    //!< Relative humidity at 500 hPa
  Idx_HUMID_RE300,    //!< Relative humidity at 300 hPa
  Idx_GEOP_HGT,       //!< Surface geopotential height
  Idx_GEOP_HGT850,    //!< Geopotential height at 850 hPa
  Idx_GEOP_HGT700,    //!< Geopotential height at 700 hPa
  Idx_GEOP_HGT500,    //!< Geopotential height at 500 hPa
  Idx_GEOP_HGT300,    //!< Geopotential height at 300 hPa
  Idx_COUNT           //!< Number of supported GRIB record types
};

/**
 * Manages a collection of GribRecord objects representing multiple
 * meteorological parameters at a single point in time.
 *
 * While a GribRecord represents a single parameter's grid data (e.g.,
 * temperature or wind-x component), GribRecordSet bundles related records
 * together to provide a complete meteorological picture for one timestamp. For
 * example, it might contain:
 *
 * - Surface winds (both X and Y components)
 * - Wind components at different pressure levels (850hPa, 700hPa, etc.)
 * - Temperature, pressure, humidity
 * - Wave and current data
 *
 * Records are stored in a fixed-size array where each index corresponds to a
 * specific parameter as defined by the Idx_* enumeration. The set can either:
 * - Own records (usually interpolated records it created)
 * - Reference records (owned by the GRIB file reader)
 *
 * @note This class handles memory management for records it owns via
 * m_GribRecordUnref
 */
class GribRecordSet {
public:
  /**
   * Creates an empty record set.
   *
   * @param id Unique identifier for this record set.
   *           The id is used to uniquely identify this record set within a GRIB
   * file.
   */
  GribRecordSet(unsigned int id) : m_Reference_Time(-1), m_ID(id) {
    for (int i = 0; i < Idx_COUNT; i++) {
      m_GribRecordPtrArray[i] = 0;
      m_GribRecordUnref[i] = false;
    }
  }

  virtual ~GribRecordSet() { RemoveGribRecords(); }

  /**
   * Sets a GRIB record that this set owns and will be responsible for deleting.
   *
   * Typically used for interpolated records created between two time points.
   * The set takes ownership and will delete the record when appropriate.
   *
   * @param i Index in the record array where to store the record
   * @param pGR Pointer to the GRIB record to store
   */
  void SetUnRefGribRecord(int i, GribRecord* pGR) {
    assert(i >= 0 && i < Idx_COUNT);
    if (m_GribRecordUnref[i] == true) {
      delete m_GribRecordPtrArray[i];
    }
    m_GribRecordPtrArray[i] = pGR;
    m_GribRecordUnref[i] = true;
  }

  /**
   * Removes and deletes all GRIB records owned by this set.
   *
   * Records not owned by this set are left untouched.
   */
  void RemoveGribRecords() {
    for (int i = 0; i < Idx_COUNT; i++) {
      if (m_GribRecordUnref[i] == true) {
        delete m_GribRecordPtrArray[i];
      }
    }
  }

  /**
   * Gets a GRIB record for a specific parameter index.
   *
   * This method provides controlled access to the GribRecord array, replacing
   * direct array access.
   *
   * @param idx Parameter index (see Idx_* enum).
   * @return Pointer to GribRecord if available and valid, nullptr otherwise.
   */
  GribRecord* GetRecord(int idx);
  void SetRecord(int idx, GribRecord* pGR);

  /**
   * Gets a read-only GRIB record for a specific parameter index.
   *
   * @param idx Parameter index (see Idx_* enum).
   * @return Pointer to GribRecord if available and valid, nullptr otherwise.
   */
  virtual const GribRecord* GetRecord(int idx) const;

  time_t GetReferenceTime() const { return m_Reference_Time; }

  void SetReferenceTime(time_t refTime) { m_Reference_Time = refTime; }

private:
  /**
   * Reference time for all records in this set.
   *
   * This timestamp represents when the forecast data in this record set is
   * valid, which is: reference_time (model initialization time) +
   * forecast_period.
   * For example, if a model was initialized at 2024-02-11 00:00Z and this is
   * the +6h forecast, this field would contain 2024-02-11 06:00Z.
   *
   * All GRIB records in this set (winds, pressure, temperature, etc.) share
   * this same valid time.
   *
   * @note Despite the name, this is not the GRIB reference time (model
   * initialization time)
   */
  time_t m_Reference_Time;

  /** Unique identifier for this record set. */
  unsigned int m_ID;

  /**
   * Array of pointers to GRIB records representing different meteorological
   * parameters.
   *
   * Each index corresponds to a specific parameter (wind, gust, wave, etc).
   * Records may be owned by this set (tracked by m_GribRecordUnref) or
   * referenced from elsewhere.
   */
  GribRecord* m_GribRecordPtrArray[Idx_COUNT];

  // grib records files are stored and owned by reader mapGribRecords
  // interpolated grib are not, keep track of them
  bool m_GribRecordUnref[Idx_COUNT];
};

/**
 * Dynamic Array of GRIB Record Sets.
 *
 * A wxWidgets-based array managing multiple GribRecordSet objects, where each
 * set contains meteorological parameters valid at a specific forecast time.
 * This array typically holds the complete time sequence of weather data for a
 * GRIB file.
 *
 * Technical Details:
 * - Implemented using WX_DECLARE_OBJARRAY which creates a dynamic array of
 * objects
 * - Each element is a GribRecordSet containing up to Idx_COUNT different
 * parameters
 * - Array elements are typically ordered by forecast time
 * - The array owns its GribRecordSet objects and manages their lifecycle
 *
 * Example Structure:
 * [0] -> GribRecordSet for T+0h (analysis)
 *   - Surface wind (U,V components)
 *   - Upper air winds (850hPa, 700hPa, etc.)
 *   - Temperature, pressure, waves
 * [1] -> GribRecordSet for T+3h
 * [2] -> GribRecordSet for T+6h
 * ...
 *
 */
WX_DECLARE_OBJARRAY(GribRecordSet, ArrayOfGribRecordSets);

#endif  // GRIB_RECORD_SET_H