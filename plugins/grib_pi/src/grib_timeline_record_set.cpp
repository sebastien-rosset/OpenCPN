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
#include <wx/log.h>

#include "grib_timeline_record_set.h"
#include "IsoLine.h"

/* interpolating constructor
   as a possible optimization, write this function to also
   take latitude longitude boundaries so the resulting record can be
   a subset of the input, but also would need to be recomputed when panning the
   screen */
GribTimelineRecordSet::GribTimelineRecordSet(unsigned int cnt)
    : GribRecordSet(cnt) {
  for (int i = 0; i < Idx_COUNT; i++) m_IsobarArray[i] = nullptr;
}

GribTimelineRecordSet::~GribTimelineRecordSet() {
  // RemoveGribRecords();
  ClearCachedData();
}

void GribTimelineRecordSet::ClearCachedData() {
  for (int i = 0; i < Idx_COUNT; i++) {
    if (m_IsobarArray[i]) {
      // Clear out the cached isobars
      for (unsigned int j = 0; j < m_IsobarArray[i]->GetCount(); j++) {
        IsoLine *piso = (IsoLine *)m_IsobarArray[i]->Item(j);
        delete piso;
      }
      delete m_IsobarArray[i];
      m_IsobarArray[i] = nullptr;
    }
  }
}