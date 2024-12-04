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

#include "grib_layer_manager.h"

GribLayerManager::GribLayerManager() {}

GribLayerManager::~GribLayerManager() { ClearAllLayers(); }

// Initial implementation - treat as single layer for compatibility
bool GribLayerManager::IsOK() const { return m_layers[0].IsOK(); }

void GribLayerManager::AssignLayer(int index, GRIBFile* file) {
  assert(index >= 0 && index < MAX_LAYERS);
  if (index >= 0 && index < MAX_LAYERS) {
    m_layers[index].Assign(file);
  }
}

void GribLayerManager::ClearLayer(int index) {
  assert(index >= 0 && index < MAX_LAYERS);
  if (index >= 0 && index < MAX_LAYERS) {
    m_layers[index].Clear();
  }
}

void GribLayerManager::ClearAllLayers() {
  for (int i = 0; i < MAX_LAYERS; i++) {
    m_layers[i].Clear();
  }
}

time_t GribLayerManager::GetRefDateTime() const {
  // Initially just returns first layer's time
  return GetLayerRefDateTime(0);
}

time_t GribLayerManager::GetLayerRefDateTime(int layerIndex) const {
  assert(layerIndex >= 0 && layerIndex < MAX_LAYERS);
  if (layerIndex >= 0 && layerIndex < MAX_LAYERS &&
      m_layers[layerIndex].IsEnabled()) {
    return m_layers[layerIndex].GetRefDateTime();
  }
  return 0;
}

time_t GribLayerManager::GetEarliestRefDateTime() const {
  time_t earliest = 0;
  for (int i = 0; i < MAX_LAYERS; i++) {
    if (m_layers[i].IsEnabled()) {
      time_t layerTime = m_layers[i].GetRefDateTime();
      if (!earliest || layerTime < earliest) {
        earliest = layerTime;
      }
    }
  }
  return earliest;
}

time_t GribLayerManager::GetLatestRefDateTime() const {
  time_t latest = 0;
  for (int i = 0; i < MAX_LAYERS; i++) {
    if (m_layers[i].IsEnabled()) {
      time_t layerTime = m_layers[i].GetRefDateTime();
      if (layerTime > latest) {
        latest = layerTime;
      }
    }
  }
  return latest;
}