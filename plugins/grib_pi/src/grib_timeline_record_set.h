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

#ifndef __GRIB_TIMELINE_RECORD_SET_H__
#define __GRIB_TIMELINE_RECORD_SET_H__

#include "GribRecord.h"
#include "GribRecordSet.h"
#include "grib_file.h"

/**
 * A specialized GribRecordSet that represents temporally interpolated weather
 * data with isobar rendering optimizations.
 *
 * While GribRecordSet simply holds meteorological parameters at a point in
 * time, GribTimelineRecordSet provides:
 * 1. Temporally interpolated records between two time points.
 * 2. Cached calculations for visualization (e.g., isobars).
 *
 * This class is used when:
 * - Displaying weather conditions between available GRIB timestamps.
 * - Animating weather evolution over time.
 * - Rendering isobars, isotherms, and other derived visualizations.
 *
 * @see GribRecordSet for basic parameter storage.
 * @see GetTimeLineRecordSet() for how interpolation is performed.
 */

class GribTimelineRecordSet : public GribRecordSet {
public:
  /**
   * Creates a timeline record set containing temporally interpolated GRIB
   * records.
   *
   * Timeline record sets store cached data like isobar calculations to optimize
   * rendering performance during animation playback.
   *
   * @param cnt Source GRIB file identifier used to trace record origins
   */
  GribTimelineRecordSet(unsigned int cnt);
  //    GribTimelineRecordSet(GribRecordSet &GRS1, GribRecordSet &GRS2, double
  //    interp_const);
  ~GribTimelineRecordSet();

  void ClearCachedData();

  /**
   * Array of cached isobar calculations for each data type (wind, pressure,
   * etc).
   *
   * Each element is a pointer to a wxArrayPtrVoid containing IsoLine objects.
   * Used to speed up rendering by avoiding recalculation of isobars.
   */
  wxArrayPtrVoid *m_IsobarArray[Idx_COUNT];
};

#endif  // __GRIB_TIMELINE_RECORD_SET_H__