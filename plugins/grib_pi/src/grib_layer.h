/******************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  Multi-layer GRIB file support
 *
 ***************************************************************************
 *   Copyright (C) 2024                                                    *
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
 ***************************************************************************
 */

#ifndef _GRIB_LAYER_H_
#define _GRIB_LAYER_H_

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif  // precompiled headers

class GRIBFile;
class ArrayOfGribRecordSets;

/**
 * GribLayer encapsulates a single GRIB file.
 */
class GribLayer {
public:
  GribLayer();
  ~GribLayer();

  /** Enable or disable this grib layer. */
  void SetEnabled(bool enabled);
  /** Return true if this grib layer is enabled. */
  bool IsEnabled() const;

  /** Return the grib file assigned to this layer. */
  GRIBFile* GetGribFile() const;
  /** Return true if a grib file is assigned and the GRIB data is valid. */
  bool IsOK() const;
  /** Set the GRIB file for this layer. */
  void Assign(GRIBFile* file);
  /** Clear the GRIB file assigned to this layer. */
  void Clear();

  int GetGribIndex(int recordType) const;

  ArrayOfGribRecordSets* GetRecordSetArrayPtr() const;

  wxArrayString GetFileNames() const;

  time_t GetRefDateTime() const;

private:
  bool m_bEnabled;        /// Whether this layer is currently enabled.
  GRIBFile* m_pGribFile;  /// The GRIB file associated with this layer.
};

#endif  // _GRIB_LAYER_H_
