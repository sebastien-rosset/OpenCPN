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

#include "grib_file.h"

//----------------------------------------------------------------------------------------------------------
//          GRIBFile Object Implementation
//----------------------------------------------------------------------------------------------------------
unsigned int GRIBFile::ID = 0;

GRIBFile::GRIBFile(const wxArrayString &file_names, bool CumRec, bool WaveRec,
                   bool newestFile)
    : m_counter(++ID) {
  m_bOK = false;  // Assume ok until proven otherwise
  m_pGribReader = nullptr;
  m_last_message = wxEmptyString;
  for (unsigned int i = 0; i < file_names.GetCount(); i++) {
    wxString file_name = file_names[i];
    if (::wxFileExists(file_name)) m_bOK = true;
  }

  if (m_bOK == false) {
    m_last_message = _(" files don't exist!");
    return;
  }
  //    Use the zyGrib support classes, as (slightly) modified locally....
  m_pGribReader = new GribReader();

  //    Read and ingest the entire GRIB file.......
  m_bOK = false;
  wxString file_name;
  for (unsigned int i = 0; i < file_names.GetCount(); i++) {
    file_name = file_names[i];
    m_pGribReader->openFile(file_name);

    if (m_pGribReader->isOk()) {
      m_bOK = true;
      if (newestFile) {
        break;
      }
    }
  }
  if (m_bOK == false) {
    m_last_message = _(" can't be read!");
    return;
  }

  if (newestFile) {
    m_FileNames.Clear();
    m_FileNames.Add(file_name);
  } else {
    m_FileNames = file_names;
  }

  // fixup Accumulation records
  m_pGribReader->computeAccumulationRecords(GRB_PRECIP_TOT, LV_GND_SURF, 0);
  m_pGribReader->computeAccumulationRecords(GRB_PRECIP_RATE, LV_GND_SURF, 0);
  m_pGribReader->computeAccumulationRecords(GRB_CLOUD_TOT, LV_ATMOS_ALL, 0);

  if (CumRec)
    m_pGribReader->copyFirstCumulativeRecord();  // add missing records if
                                                 // option selected
  if (WaveRec)
    m_pGribReader->copyMissingWaveRecords();  //  ""                   ""

  m_nGribRecords = m_pGribReader->getTotalNumberOfGribRecords();

  //    Walk the GribReader date list to populate our array of GribRecordSets

  std::set<time_t>::iterator iter;
  std::set<time_t> date_list = m_pGribReader->getListDates();
  for (iter = date_list.begin(); iter != date_list.end(); iter++) {
    GribRecordSet *t = new GribRecordSet(m_counter);
    time_t reftime = *iter;
    t->SetReferenceTime(reftime);
    m_GribRecordSetArray.Add(t);
  }

  //    Convert from zyGrib organization by data type/level to our organization
  //    by time.

  GribRecord *pRec;
  bool isOK(false);
  bool polarWind(false);
  bool polarCurrent(false);
  bool sigWave(false);
  bool sigH(false);
  //    Get the map of GribRecord vectors
  std::map<std::string, std::vector<GribRecord *> *> *p_map =
      m_pGribReader->getGribMap();

  //    Iterate over the map to get vectors of related GribRecords
  std::map<std::string, std::vector<GribRecord *> *>::iterator it;
  for (it = p_map->begin(); it != p_map->end(); it++) {
    std::vector<GribRecord *> *ls = (*it).second;
    for (zuint i = 0; i < ls->size(); i++) {
      pRec = ls->at(i);
      isOK = true;
      time_t thistime = pRec->getRecordCurrentDate();

      //   Search the GribRecordSet array for a GribRecordSet with matching time
      for (unsigned int j = 0; j < m_GribRecordSetArray.GetCount(); j++) {
        if (m_GribRecordSetArray.Item(j).GetReferenceTime() == thistime) {
          int idx = -1, mdx = -1;
          switch (pRec->getDataType()) {
            case GRB_WIND_DIR:
              polarWind = true;
              // fall through
            case GRB_WIND_VX:
              if (pRec->getLevelType() == LV_ISOBARIC) {
                switch (pRec->getLevelValue()) {
                  case 300:
                    idx = Idx_WIND_VX300;
                    break;
                  case 500:
                    idx = Idx_WIND_VX500;
                    break;
                  case 700:
                    idx = Idx_WIND_VX700;
                    break;
                  case 850:
                    idx = Idx_WIND_VX850;
                    break;
                }
              } else
                idx = Idx_WIND_VX;
              break;
            case GRB_WIND_SPEED:
              polarWind = true;
              // fall through
            case GRB_WIND_VY:
              if (pRec->getLevelType() == LV_ISOBARIC) {
                switch (pRec->getLevelValue()) {
                  case 300:
                    idx = Idx_WIND_VY300;
                    break;
                  case 500:
                    idx = Idx_WIND_VY500;
                    break;
                  case 700:
                    idx = Idx_WIND_VY700;
                    break;
                  case 850:
                    idx = Idx_WIND_VY850;
                    break;
                }
              } else
                idx = Idx_WIND_VY;
              break;
            case GRB_CUR_DIR:
              polarCurrent = true;
              // fall through
            case GRB_UOGRD:
              idx = Idx_SEACURRENT_VX;
              break;
            case GRB_CUR_SPEED:
              polarCurrent = true;
              // fall through
            case GRB_VOGRD:
              idx = Idx_SEACURRENT_VY;
              break;
            case GRB_WIND_GUST:
              idx = Idx_WIND_GUST;
              break;
            case GRB_PRESSURE:
              idx = Idx_PRESSURE;
              break;
            case GRB_HTSGW:
              sigH = true;
              idx = Idx_HTSIGW;
              break;
            case GRB_PER:
              sigWave = true;
              idx = Idx_WVPER;
              break;
            case GRB_DIR:
              sigWave = true;
              idx = Idx_WVDIR;
              break;
            case GRB_WVHGT:
              idx = Idx_HTSIGW;
              break;  // Translation from NOAA WW3
            case GRB_WVPER:
              idx = Idx_WVPER;
              break;
            case GRB_WVDIR:
              idx = Idx_WVDIR;
              break;
            case GRB_PRECIP_RATE:
            case GRB_PRECIP_TOT:
              idx = Idx_PRECIP_TOT;
              break;
            case GRB_CLOUD_TOT:
              idx = Idx_CLOUD_TOT;
              break;
            case GRB_TEMP:
              if (pRec->getLevelType() == LV_ISOBARIC) {
                switch (pRec->getLevelValue()) {
                  case 300:
                    idx = Idx_AIR_TEMP300;
                    break;
                  case 500:
                    idx = Idx_AIR_TEMP500;
                    break;
                  case 700:
                    idx = Idx_AIR_TEMP700;
                    break;
                  case 850:
                    idx = Idx_AIR_TEMP850;
                    break;
                }
              } else
                idx = Idx_AIR_TEMP;
              if (pRec->getDataCenterModel() == NORWAY_METNO)
                mdx = 1000 + NORWAY_METNO;
              break;
            case GRB_WTMP:
              idx = Idx_SEA_TEMP;
              if (pRec->getDataCenterModel() == NOAA_GFS) mdx = 1000 + NOAA_GFS;
              break;
            case GRB_CAPE:
              idx = Idx_CAPE;
              break;
            case GRB_COMP_REFL:
              idx = Idx_COMP_REFL;
              break;
            case GRB_HUMID_REL:
              if (pRec->getLevelType() == LV_ISOBARIC) {
                switch (pRec->getLevelValue()) {
                  case 300:
                    idx = Idx_HUMID_RE300;
                    break;
                  case 500:
                    idx = Idx_HUMID_RE500;
                    break;
                  case 700:
                    idx = Idx_HUMID_RE700;
                    break;
                  case 850:
                    idx = Idx_HUMID_RE850;
                    break;
                }
              }
              break;
            case GRB_GEOPOT_HGT:
              if (pRec->getLevelType() == LV_ISOBARIC) {
                switch (pRec->getLevelValue()) {
                  case 300:
                    idx = Idx_GEOP_HGT300;
                    break;
                  case 500:
                    idx = Idx_GEOP_HGT500;
                    break;
                  case 700:
                    idx = Idx_GEOP_HGT700;
                    break;
                  case 850:
                    idx = Idx_GEOP_HGT850;
                    break;
                }
              }
              break;
          }
          if (idx == -1) {
            // XXX bug ?
            break;
          }

          bool skip = false;

          if (m_GribRecordSetArray.Item(j).GetRecord(idx)) {
            // already one
            GribRecord *oRec = m_GribRecordSetArray.Item(j).GetRecord(idx);
            if (idx == Idx_PRESSURE) {
              skip = (oRec->getLevelType() == LV_MSL);
            } else {
              // we favor UV over DIR/SPEED
              if (polarWind) {
                if (oRec->getDataType() == GRB_WIND_VY ||
                    oRec->getDataType() == GRB_WIND_VX)
                  skip = true;
              }
              if (polarCurrent) {
                if (oRec->getDataType() == GRB_UOGRD ||
                    oRec->getDataType() == GRB_VOGRD)
                  skip = true;
              }
              // favor average aka timeRange == 3 (HRRR subhourly subsets have
              // both 3 and 0 records for winds)
              if (!skip && (oRec->getTimeRange() == 3)) {
                skip = true;
              }
              // we favor significant Wave other wind wave.
              if (sigH) {
                if (oRec->getDataType() == GRB_HTSGW) skip = true;
              }
              if (sigWave) {
                if (oRec->getDataType() == GRB_DIR ||
                    oRec->getDataType() == GRB_PER)
                  skip = true;
              }
            }
          }
          if (!skip) {
            m_GribRecordSetArray.Item(j).SetRecord(idx, pRec);
            if (m_GribIdxArray.Index(idx) == wxNOT_FOUND)
              m_GribIdxArray.Add(idx, 1);
            if (mdx != -1 && m_GribIdxArray.Index(mdx) == wxNOT_FOUND)
              m_GribIdxArray.Add(mdx, 1);
          }
          break;
        }
      }
    }
  }

  if (polarWind || polarCurrent) {
    for (unsigned int j = 0; j < m_GribRecordSetArray.GetCount(); j++) {
      for (unsigned int i = 0; i < Idx_COUNT; i++) {
        int idx = -1;
        if (polarWind) {
          GribRecord *pRec = m_GribRecordSetArray.Item(j).GetRecord(i);

          if (pRec != nullptr && pRec->getDataType() == GRB_WIND_DIR) {
            switch (i) {
              case Idx_WIND_VX300:
                idx = Idx_WIND_VY300;
                break;
              case Idx_WIND_VX500:
                idx = Idx_WIND_VY500;
                break;
              case Idx_WIND_VX700:
                idx = Idx_WIND_VY700;
                break;
              case Idx_WIND_VX850:
                idx = Idx_WIND_VY850;
                break;
              case Idx_WIND_VX:
                idx = Idx_WIND_VY;
                break;
              default:
                break;
            }
            if (idx != -1) {
              GribRecord *pRec1 = m_GribRecordSetArray.Item(j).GetRecord(idx);
              if (pRec1 != nullptr && pRec1->getDataType() == GRB_WIND_SPEED)
                GribRecord::Polar2UV(pRec, pRec1);
            }
          }
        }
        if (polarCurrent) {
          idx = -1;
          GribRecord *pRec = m_GribRecordSetArray.Item(j).GetRecord(i);

          if (pRec != nullptr && pRec->getDataType() == GRB_CUR_DIR) {
            switch (i) {
              case Idx_SEACURRENT_VX:
                idx = Idx_SEACURRENT_VY;
                break;
              default:
                break;
            }
            if (idx != -1) {
              GribRecord *pRec1 = m_GribRecordSetArray.Item(j).GetRecord(idx);
              if (pRec1 != nullptr && pRec1->getDataType() == GRB_CUR_SPEED)
                GribRecord::Polar2UV(pRec, pRec1);
            }
          }
        }
      }
    }
  }

  if (isOK)
    m_pRefDateTime =
        pRec->getRecordRefDate();  // to ovoid crash with some bad files
}

GRIBFile::~GRIBFile() { delete m_pGribReader; }