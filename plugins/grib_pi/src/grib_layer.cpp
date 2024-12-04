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

#include "grib_layer.h"
#include "GribUIDialog.h"

GribLayer::GribLayer() : m_bEnabled(false), m_pGribFile(nullptr) {}

GribLayer::~GribLayer() { Clear(); }

bool GribLayer::IsOK() const { return m_pGribFile && m_pGribFile->IsOK(); }

void GribLayer::Assign(GRIBFile* file) {
  if (m_pGribFile == file) return;
  Clear();
  m_pGribFile = file;
  m_bEnabled = file && file->IsOK();
}

void GribLayer::Clear() {
  if (m_pGribFile) delete m_pGribFile;
  m_pGribFile = nullptr;
  m_bEnabled = false;
}

int GribLayer::GetGribIndex(int recordType) const {
  return m_pGribFile ? m_pGribFile->m_GribIdxArray.Index(recordType)
                     : wxNOT_FOUND;
}

void GribLayer::SetEnabled(bool enabled) { m_bEnabled = enabled; }

bool GribLayer::IsEnabled() const { return m_bEnabled; }

GRIBFile* GribLayer::GetGribFile() const { return m_pGribFile; }

ArrayOfGribRecordSets* GribLayer::GetRecordSetArrayPtr() const {
  return m_pGribFile ? m_pGribFile->GetRecordSetArrayPtr() : nullptr;
}

wxArrayString GribLayer::GetFileNames() const {
  return m_pGribFile ? m_pGribFile->GetFileNames() : wxArrayString();
}

time_t GribLayer::GetRefDateTime() const {
  return m_pGribFile ? m_pGribFile->GetRefDateTime() : 0;
}