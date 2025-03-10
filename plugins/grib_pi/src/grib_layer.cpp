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

#include "grib_layer.h"
#include "grib_layer_set.h"
#include "GribRecord.h"
#include "GribRecordSet.h"

GRIBLayer::GRIBLayer(const wxString& name, GRIBFile* file, GRIBLayerSet* parent)
    : m_name(name), m_enabled(true), m_file(file), m_parent(parent) {}

GRIBLayer::~GRIBLayer() { delete m_timelineRecordSet; }

wxString GRIBLayer::GetLastError() const {
  return m_file ? m_file->GetLastMessage() : wxString();
}

time_t GRIBLayer::GetRefDateTime() const {
  return m_file ? m_file->GetRefDateTime() : 0;
}

void GRIBLayer::SetGribFile(GRIBFile* file) {
  m_file.reset(file);
  if (m_parent) {
    m_parent->UpdateCache();
  }
}

bool GRIBLayer::IsOK() const { return m_file && m_file->IsOK(); }

void GRIBLayer::SetEnabled(bool enabled) {
  if (m_enabled != enabled) {
    m_enabled = enabled;
    if (m_parent) {
      m_parent->UpdateCache();
    }
  }
}

wxDateTime GRIBLayer::MinTime() {
  const ArrayOfGribRecordSets* rsa = GetFile()->GetRecordSetArrayPtr();
  if (rsa && rsa->GetCount()) {
    GribRecordSet& first = rsa->Item(0);
    return first.GetReferenceTime();
  }
  return wxDateTime::Now();
}

/** Unique identifier for GRIB layer sets. */
unsigned int ID;

GribTimelineRecordSet* GRIBLayer::GetTimeLineRecordSet(wxDateTime time) {
  if (!IsOK()) return nullptr;

  // If we already calculated for this time, return cached value
  if (m_timelineRecordSet && time == m_timelineTime) {
    return m_timelineRecordSet;
  }

  // Clear any existing cached data
  if (m_timelineRecordSet) {
    delete m_timelineRecordSet;
    m_timelineRecordSet = nullptr;
  }

  const ArrayOfGribRecordSets* rsa = GetFile()->GetRecordSetArrayPtr();

  if (rsa->GetCount() == 0) return nullptr;

  GribTimelineRecordSet* set = new GribTimelineRecordSet(GetId());
  for (int i = 0; i < Idx_COUNT; i++) {
    GribRecordSet *GRS1 = nullptr, *GRS2 = nullptr;
    GribRecord *GR1 = nullptr, *GR2 = nullptr;
    wxDateTime GR1time, GR2time;

    // already computed using polar interpolation from first axis
    if (set->GetRecord(i)) continue;

    unsigned int j;
    for (j = 0; j < rsa->GetCount(); j++) {
      GribRecordSet* GRS = &rsa->Item(j);
      GribRecord* GR = GRS->GetRecord(i);
      if (!GR) continue;

      wxDateTime curtime = GRS->GetReferenceTime();
      if (curtime <= time) GR1time = curtime, GRS1 = GRS, GR1 = GR;

      if (curtime >= time) {
        GR2time = curtime, GRS2 = GRS, GR2 = GR;
        break;
      }
    }

    if (!GR1 || !GR2) continue;

    wxDateTime mintime = MinTime();
    double minute2 = (GR2time - mintime).GetMinutes();
    double minute1 = (GR1time - mintime).GetMinutes();
    double nminute = (time - mintime).GetMinutes();

    if (minute2 < minute1 || nminute < minute1 || nminute > minute2) continue;

    double interp_const;
    if (minute1 == minute2) {
      // with big grib a copy is slow use a reference.
      set->SetRecord(i, GR1);
      continue;
    } else
      interp_const = (nminute - minute1) / (minute2 - minute1);

    /* if this is a vector interpolation use the 2d method */
    if (i < Idx_WIND_VY) {
      GribRecord* GR1y = GRS1->GetRecord(i + Idx_WIND_VY);
      GribRecord* GR2y = GRS2->GetRecord(i + Idx_WIND_VY);
      if (GR1y && GR2y) {
        GribRecord* Ry;
        set->SetUnRefGribRecord(
            i, GribRecord::Interpolated2DRecord(Ry, *GR1, *GR1y, *GR2, *GR2y,
                                                interp_const));
        set->SetUnRefGribRecord(i + Idx_WIND_VY, Ry);
        continue;
      }
    } else if (i <= Idx_WIND_VY300)
      continue;
    else if (i == Idx_SEACURRENT_VX) {
      GribRecord* GR1y = GRS1->GetRecord(Idx_SEACURRENT_VY);
      GribRecord* GR2y = GRS2->GetRecord(Idx_SEACURRENT_VY);
      if (GR1y && GR2y) {
        GribRecord* Ry;
        set->SetUnRefGribRecord(
            i, GribRecord::Interpolated2DRecord(Ry, *GR1, *GR1y, *GR2, *GR2y,
                                                interp_const));
        set->SetUnRefGribRecord(Idx_SEACURRENT_VY, Ry);
        continue;
      }
    } else if (i == Idx_SEACURRENT_VY)
      continue;

    set->SetUnRefGribRecord(i, GribRecord::InterpolatedRecord(
                                   *GR1, *GR2, interp_const, i == Idx_WVDIR));
  }

  set->SetReferenceTime(time.GetTicks());
  //(1-interp_const)*GRS1.m_Reference_Time + interp_const*GRS2.m_Reference_Time;
  m_timelineRecordSet = set;
  return set;
}