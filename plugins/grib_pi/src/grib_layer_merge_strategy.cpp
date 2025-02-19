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

#include "grib_layer_set.h"
#include "GribRecord.h"
#include "GribRecordSet.h"

std::vector<double> LayerMergeStrategy::ScoreRecords(
    const std::vector<GribRecord*>& recordOptions, time_t targetTime) const {
  std::vector<double> scores(recordOptions.size());

  // Get current time
  time_t now = time(nullptr);
  const double MAX_AGE_HOURS = 72.0;  // 3 days max age

  for (size_t i = 0; i < recordOptions.size(); i++) {
    GribRecord* record = recordOptions[i];
    double score = 0.0;

    switch (m_scoringMethod) {
      case ScoringMethod::CURRENCY_FIRST: {
        // Calculate temporal currency score (0.0-1.0)
        double ageHours = difftime(now, record->getRecordRefDate()) / 3600.0;
        double currencyScore = 1.0 - std::min(1.0, ageHours / MAX_AGE_HOURS);

        // Calculate resolution score (0.0-1.0)
        double resolution = std::max(record->getDi(), record->getDj());
        double resolutionScore;
        if (resolution <= 0.1)
          resolutionScore = 1.0;
        else if (resolution <= 0.5)
          resolutionScore = 0.8;
        else
          resolutionScore = 0.6;

        // Combine scores with currency weighted higher
        score = (0.7 * currencyScore) + (0.3 * resolutionScore);
        break;
      }

      case ScoringMethod::RESOLUTION_FIRST: {
        // Calculate resolution score (0.0-1.0)
        double resolution = std::max(record->getDi(), record->getDj());
        double resolutionScore;
        if (resolution <= 0.1)
          resolutionScore = 1.0;
        else if (resolution <= 0.5)
          resolutionScore = 0.8;
        else
          resolutionScore = 0.6;

        // Calculate temporal currency score (0.0-1.0)
        double ageHours = difftime(now, record->getRecordRefDate()) / 3600.0;
        double currencyScore = 1.0 - std::min(1.0, ageHours / MAX_AGE_HOURS);

        // Combine scores with resolution weighted higher
        score = (0.7 * resolutionScore) + (0.3 * currencyScore);
        break;
      }

      case ScoringMethod::MODEL_QUALITY: {
        // Get model type from record
        DataCenterModel model =
            static_cast<DataCenterModel>(record->getDataCenterModel());

        // Default model quality scores based on enum values
        double modelScore;
        switch (model) {
          case NOAA_HRRR: {
            // HRRR is highest resolution at 3km
            double forecastHours =
                difftime(targetTime, record->getRecordRefDate()) / 3600.0;
            time_t refTime = record->getRecordRefDate();
            struct tm* refTm = gmtime(&refTime);

            if (forecastHours <= 18.0)
              modelScore = 1.0;  // Best for short-range
            else if (forecastHours <= 48.0 &&
                     (refTm->tm_hour == 0 || refTm->tm_hour == 6 ||
                      refTm->tm_hour == 12 || refTm->tm_hour == 18))
              modelScore =
                  0.95;  // Extended runs only available for 00/06/12/18Z
            else
              modelScore = 0.6;  // Beyond design range
            break;
          }
          case NOAA_GFS: {
            // GFS quality depends on forecast length
            double forecastHours =
                difftime(targetTime, record->getRecordRefDate()) / 3600.0;
            modelScore = (forecastHours <= 72.0) ? 0.9 : 0.8;
            break;
          }
          case ECMWF_ERA5:
            modelScore = 1.0;  // High quality reanalysis
            break;
          case KNMI_HIRLAM:
          case KNMI_HARMONIE_AROME:
            modelScore = 0.95;  // High resolution regional models
            break;
          case NOAA_NCEP_WW3:
          case FNMOC_WW3_GLB:
          case FNMOC_WW3_MED:
            modelScore = 0.9;  // Wave models
            break;
          case NOAA_RTOFS:
            modelScore = 0.85;  // Ocean model
            break;
          case NORWAY_METNO:
            modelScore = 0.8;  // Regional model
            break;
          case OTHER_DATA_CENTER:
          default:
            modelScore = 0.7;  // Unknown models get lower priority
            break;
        }

        score = modelScore;
        break;
      }

      case ScoringMethod::USER_PRIORITY:
        // In this case, we rely solely on the order of records in the input
        // vector Earlier records (from higher priority layers) get higher
        // scores
        score = 1.0 - (static_cast<double>(i) / recordOptions.size());
        break;
    }

    scores[i] = score;
  }

  return scores;
}

void LayerMergeStrategy::SetScoringMethod(ScoringMethod method) {
  m_scoringMethod = method;
}

bool LayerMergeStrategy::ValidateMerge(
    const std::vector<GribRecord*>& selectedRecords) const {
  return true;  // placeholder
}