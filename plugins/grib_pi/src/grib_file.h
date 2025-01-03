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

#ifndef __GRIB_FILE_H__
#define __GRIB_FILE_H__

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif  // precompiled headers

#include "ocpn_plugin.h"

#include "GribRecordSet.h"
#include "GribReader.h"

/**
 * Manages multiple GRIB record sets from one or more GRIB files.
 *
 * GRIBFile is responsible for parsing and organizing weather data from GRIB
 * files. It can handle:
 * - Multiple files with different data types (e.g., wind in one file, waves in
 * another).
 * - Multiple files with overlapping data.
 * - Data from different meteorological models and levels.
 *
 * The class provides logic for resolving overlapping or conflicting records by:
 * - Favoring UV vector components over polar (direction/speed) representations.
 * - Preferring mean/average records over instantaneous values.
 * - Using Mean-Sea-Level (MSL) pressure over other pressure types.
 * - Prioritizing significant wave data over wind wave data.
 */
class DECL_EXP GRIBFile {
public:
  /**
   * Creates a new GRIBFile by parsing one or more GRIB files.
   *
   * @param file_names Array of GRIB file paths to load. Can contain multiple
   * files with different or overlapping data types.
   * @param CumRec Whether to copy first cumulative record to fill gaps in
   * precipitation and cloud cover data, preventing artificial zero periods.
   * @param WaveRec Whether to copy missing wave records to fill gaps, ensuring
   *                continuous marine condition visualization.
   * @param newestFile When true, only load the newest file from the array.
   *                  When false (default), combine all records from all files.
   */
  GRIBFile(const wxArrayString &file_names, bool CumRec, bool WaveRec,
           bool newestFile = false);
  ~GRIBFile();

  // Prevent copying
  GRIBFile(const GRIBFile &) = delete;
  GRIBFile &operator=(const GRIBFile &) = delete;

  /**
   * Checks if file loading and parsing was successful.
   * @return true if at least one valid GRIB record was loaded.
   */
  bool IsOK(void) const noexcept { return m_bOK; }
  /**
   * Gets the list of source filenames being used.
   *
   * When newestFile=true, will contain only the newest file.
   * Otherwise contains all input files.
   *
   * @return Const reference to filenames array
   */
  const wxArrayString &GetFileNames(void) const noexcept { return m_FileNames; }
  /**
   * Gets the last error message if file loading failed.
   *
   * @return Error message string
   */
  wxString GetLastMessage(void) const noexcept { return m_last_message; }
  /**
   * Gets pointer to array of record sets organized by timestamp.
   * Contains combined data from all source files (or just newest file
   * if newestFile=true).
   *
   * @note The returned pointer remains owned by GRIBFile
   * @return Pointer to record sets array, never null but may be empty
   */
  const ArrayOfGribRecordSets *GetRecordSetArrayPtr(void) const noexcept {
    return &m_GribRecordSetArray;
  }

  /**
   * Gets reference datetime of the GRIB data.
   *
   * The reference time is when the model run started (analysis time).
   * This differs from the forecast time of individual records which is
   * reference_time + forecast_hour. For example, a 24h forecast from a
   * 00Z model run would have reference_time=00Z and forecast_time=00Z+24h.
   *
   * @return Reference date/time as Unix timestamp
   */
  time_t GetRefDateTime(void) const noexcept { return m_pRefDateTime; }

  const unsigned int GetCounter() const noexcept { return m_counter; }

  /**
   * Array of GRIB Parameter Indices.
   *
   * A structured array of integers, each representing a unique GRIB parameter
   * and its vertical level information. This array is used to track what
   * meteorological parameters are available in a GRIB file or layer.
   *
   * Technical Details:
   * - Implemented using wxWidgets' WX_DEFINE_ARRAY_INT macro which creates a
   * dynamic array of integers
   * - Each integer element is a packed 32-bit value encoding three pieces of
   * information:
   *   - dataType (8 bits): Type of meteorological parameter (e.g., wind,
   * temperature)
   *   - levelType (8 bits): Type of vertical reference (e.g., height above
   * ground, pressure level)
   *   - levelValue (16 bits): Specific value for the level type:
   *     - For LV_ISOBARIC: Pressure in hectopascals (e.g., 850)
   *     - For LV_ABOV_MSL/LV_ABOV_GND: Height in meters
   *     - For LV_GND_SURF/LV_MSL: Typically 0
   *
   * Example Values:
   * - Surface wind: dataType=GRB_WIND_VX, levelType=LV_GND_SURF, levelValue=0
   * - Temperature at 850hPa: dataType=GRB_TEMP, levelType=LV_ISOBARIC,
   * levelValue=850
   * - Wind at 10m: dataType=GRB_WIND_VX, levelType=LV_ABOV_GND, levelValue=10
   *
   * @see GribCode for packing/unpacking methods
   * @see GribRecord.h for dataType and levelType constant definitions
   */
  WX_DEFINE_ARRAY_INT(int, GribIdxArray);

  /**
   * Gets array of indices indicating which GRIB record types are available.
   * Each index corresponds to an Idx_ enum value indicating data presence.
   * @return Const reference to index array
   */
  const GribIdxArray &GetGribIdxArray() const noexcept {
    return m_GribIdxArray;
  }

private:
  static unsigned int ID;  //!< Unique identifier counter for GRIBFile instances

  const unsigned int m_counter;  //!< This instance's unique ID
  bool m_bOK;                    //!< Whether file loading succeeded
  wxString m_last_message;       //!< Error message if loading failed
  wxArrayString m_FileNames;     //!< Source GRIB filenames
  GribReader *m_pGribReader;     //!< Parser for GRIB file format
  time_t m_pRefDateTime;         //!< Reference time of the model run

  /** An array of GribRecordSets found in this GRIB file. */
  ArrayOfGribRecordSets m_GribRecordSetArray;

  /**
   * Array of indices indicating which GRIB record types (wind, pressure, etc.)
   * are available.
   *
   * Each index corresponds to an Idx_ enum value (e.g., Idx_WIND_VX,
   * Idx_PRESSURE) indicating that type of data exists in this file. The array
   * will not contain duplicate indices, serving as a set of available data
   * types.
   *
   * This helps quickly determine what data is available without scanning
   * through all records. For example, when deciding which UI elements to
   * enable/disable or what data to try interpolating.
   */
  GribIdxArray m_GribIdxArray;

  int m_nGribRecords;
};

#endif  // __GRIB_FILE_H__