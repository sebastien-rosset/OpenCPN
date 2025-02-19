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
#include <wx/arrimpl.cpp>

#include "GribRecordSet.h"

WX_DEFINE_OBJARRAY(ArrayOfGribRecordSets);

GribRecord* GribRecordSet::GetRecord(int idx) {
  if (idx < 0 || idx >= Idx_COUNT) {
    return nullptr;
  }
  return m_GribRecordPtrArray[idx];
}

const GribRecord* GribRecordSet::GetRecord(int idx) const {
  if (idx < 0 || idx >= Idx_COUNT) {
    return nullptr;
  }
  return m_GribRecordPtrArray[idx];
}

void GribRecordSet::SetRecord(int idx, GribRecord* pGR) {
  if (idx < 0 || idx >= Idx_COUNT) {
    return;
  }
  m_GribRecordPtrArray[idx] = pGR;
}