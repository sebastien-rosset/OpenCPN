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

#ifndef _GRIB_LAYER_MANAGER_H_
#define _GRIB_LAYER_MANAGER_H_

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif  // precompiled headers

#include "GribUIDialog.h"
#include "grib_layer.h"

class GribTimelineRecordSet;

/**
 * GribLayerManager manages multiple GRIB layers.
 */
class GribLayerManager {
public:
  GribLayerManager();
  ~GribLayerManager();

  // Layer management
  void EnableLayer(int layerIndex, bool enable);
  bool IsLayerEnabled(int layerIndex) const;
  void ClearLayer(int layerIndex);
  void ClearAllLayers();
  /** Assign the GRIBFile to the specified layer. */
  void AssignLayer(int index, GRIBFile* file);

  // Data access methods that consider all enabled layers
  GribTimelineRecordSet* GetTimelineRecordSet(wxDateTime time);
  double GetTimeInterpolatedValue(int recordType, double lat, double lon,
                                  wxDateTime time);
  bool GetTimeInterpolatedValues(double& magnitude, double& angle,
                                 int recordTypeX, int recordTypeY, double lat,
                                 double lon, wxDateTime time);

  // Get individual layer data if needed.
  GribTimelineRecordSet* GetLayerTimelineRecordSet(int layerIndex,
                                                   wxDateTime time);

  // Methods to maintain compatibility with existing code.
  // Returns highest priority enabled layer's file.
  GRIBFile* GetCurrentGribFile() const;

  // Legacy API - temporary compatibility
  time_t GetRefDateTime() const;

  /** Get reference time for a specific layer. */
  time_t GetLayerRefDateTime(int layerIndex) const;

  /** Get earliest ref time across enabled layers. */
  time_t GetEarliestRefDateTime() const;

  /** Get latest ref time across enabled layers. */
  time_t GetLatestRefDateTime() const;

  /** Return true if at least one layer is enabled and OK. */
  bool IsOK() const;

private:
  /** Maximum number of supported layers. */
  static const int MAX_LAYERS = 6;
  /** Fixed array of grib layers. */
  GribLayer m_layers[MAX_LAYERS];
};

#endif  // _GRIB_LAYER_MANAGER_H_
