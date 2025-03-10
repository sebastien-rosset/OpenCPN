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

#ifndef __GRIB_LAYER_MERGE_STRATEGY_H__
#define __GRIB_LAYER_MERGE_STRATEGY_H__

#include <vector>

#include "GribRecord.h"

/**
 * Strategy for merging GRIB records from multiple layers.
 *
 * @section overview Overview
 * This class defines how to select and combine GRIB records when multiple
 * enabled layers contain overlapping meteorological parameters, temporal
 * ranges, or spatial coverage.
 *
 * The GRIB plugin supports loading multiple GRIB files as independent layers.
 * Each layer typically represents a different weather model, time period, or
 * geographic region. When displaying or analyzing weather data, the plugin
 * needs to determine which data to use when multiple layers contain overlapping
 * information.
 *
 * @section architecture Architecture
 * When displaying or analyzing weather data (e.g., route planning), the plugin
 * determines which data to use through a merge algorithm that considers:
 * - Model quality and characteristics.
 * - Temporal resolution and currency.
 * - Spatial resolution and coverage.
 * - Parameter availability.
 * - User-defined layer priorities.
 *
 * The merge algorithm aims to:
 * 1. Seamlessly transition from high-res short-term to lower-res long-range
 * forecasts.
 * 2. Combine complementary data sources (e.g., atmospheric + wave models).
 * 3. Fallback to alternative models when preferred data is unavailable.
 * 4. Allow user control over data source preferences.
 *
 * @section usecases Use Cases
 * The merge algorithm is useful for:
 * - Optimal route planning using best available data throughout the voyage.
 * - Tactical weather decisions combining multiple forecast sources.
 * - Ensuring consistent visualization when multiple layers are enabled.
 * - Comparing routes based on different weather models.
 *
 * @section examples Example Scenarios
 * @subsection example1 1. Different Reference Times
 * Two GFS model runs with different reference times:
 * - Layer 1: 3-day old run with 7-day forecast
 * - Layer 2: 1-hour old run with 7-day forecast
 * Strategy prefers Layer 2 due to currency
 *
 * @subsection example2 2. Mixed Model Types
 * - Layer 1: HRRR (high-res, short-range)
 * - Layer 2: GFS (lower-res, long-range)
 * - Layer 3: wave and ocean current data not present in other models.
 * Strategy might use HRRR for first 48h, then transition to GFS.
 * Wave data is always used when available.
 *
 * @section factors Merge Factors
 * @subsection overview Overview
 * The merge algorithm's purpose is to create a combined dataset by
 * intelligently selecting data from available layers.
 * The algorithm processes each required time point and parameter, scoring
 * available options using configurable metrics to select optimal data. The
 * result is a merged record set that combines the strengths of each input layer
 * while maintaining meteorological consistency.
 *
 * @subsection temporal Time-based Factors
 * - Reference time currency, e.g., newer forecasts preferred.
 * - Temporal resolution, e.g., hourly vs 3-hourly data.
 * - Forecast range, e.g., some models better for short vs long-range.
 *
 * @subsection spatial Spatial Factors
 * - Resolution, e.g., 0.1° vs 0.5° grid spacing.
 * - Geographic coverage, e.g., full vs partial area.
 *
 * @subsection quality Quality Factors
 * - Model source, e.g., HRRR, GFS, ECMWF.
 * - Parameter completeness, e.g., some models lack certain parameters.
 * - User-defined layer priority, e.g., prefer HRRR over GFS.
 *
 * @section algorithm Merge Algorithm
 * Input:
 * - Vector of enabled GRIBLayers (ordered by user-defined priority)
 * - Selected merge strategy
 * - Optional user preferences
 *
 * Layer Priority Definition:
 * - Explicit order in layer set (user-controlled)
 * - Earlier layers have higher priority when other factors are equal
 *
 * @subsection metrics Quality Metrics (in order of precedence)
 * 1. Parameter Availability (binary)
 *    - Parameter exists: Consider layer
 *    - Parameter missing: Skip layer
 *
 * 2. Temporal Coverage (binary)
 *    - Forecast time covered: Consider layer
 *    - Time not covered: Skip layer
 *
 * 3. Spatial Coverage (binary)
 *    For each grid point (lat, lon):
 *    - Partial coverage: Consider layer
 *    - Area not covered: Skip layer
 *
 * 4. Temporal Currency Score (0.0-1.0)
 *    - Reference time currency (newer forecasts generally preferred).
 *    - Temporal resolution (e.g., hourly vs 3-hourly data).
 *       - Hourly: 1.0
 *       - 3-hourly: 0.8
 *       - 6-hourly: 0.6
 *    - Forecast range (some models better for short vs long-range).
 *    For each forecast hour H and GRIB reference time R:
 *    - Score = 1.0 - min(1.0, (Now - R) / MaxAllowedAge)
 *
 *    Example with MaxAllowedAge = 72 hours:
 *    GFS Model A:
 *     Reference: 2025-01-19 00:00Z (3 days old)
 *     Score = 1.0 - min(1.0, 72/72) = 0.0
 *    GFS Model B:
 *     Reference: 2025-01-21 23:00Z (1 hour old)
 *     Score = 1.0 - min(1.0, 1/72) ≈ 0.986
 *
 * 5. Resolution Score (0.0-1.0)
 *    Prefer higher resolution, e.g., 0.1° vs 0.5° grid spacing.
 *    Spatial:
 *    - ≤0.1°: 1.0
 *    - ≤0.5°: 0.8
 *    - >0.5°: 0.6
 *
 * 6. Model Quality Score (0.0-1.0)
 *    Default rankings:
 *    - HRRR: 1.0 (within first 48h)
 *    - ECMWF: 0.9
 *    - GFS: 0.8
 *    - NAM: 0.7
 *
 * 7. Combined Score (0.0-1.0)
 *   - Combined = (Temporal + Spatial) / 2
 *   - Final score = Combined * ModelQuality
 *
 * 8. Layer Order Priority
 *    - Used as final tiebreaker
 *    - Earlier layers preferred
 *
 * @subsection spatial_coverage Spatial Coverage and Grid Interpolation
 *
 * Spatial coverage is determined by the ability to interpolate data at any
 * given point (lat, lon). For each layer, we must consider:
 *
 * 1. Grid Structure
 *    - Regular latitude/longitude grid with points at fixed intervals
 *    - Grid spacing (Di, Dj) defines the resolution
 *    - Grid origin (Lo1, La1) and extent (Lo2, La2) define coverage area
 *
 * 2. Coverage Types
 *    @subsection direct Direct Grid Coverage
 *    Point falls exactly on a grid point:
 *    @code
 *    G1-----G2    G = Grid point
 *     |  P  |     P = Query point
 *    G3-----G4
 *    @endcode
 *
 *    @subsection interpolated Interpolated Coverage
 *    Point falls within a grid cell, requiring interpolation:
 *    @code
 *    G1-----G2    - Point P can be interpolated from
 *     \     /       surrounding grid points G1-G4
 *      \ P /      - Quality of interpolation depends
 *       \ /         on distance to grid points
 *    G3-----G4
 *    @endcode
 *
 *    @subsection boundary Boundary Cases
 *    Point near grid boundaries:
 *    @code
 *    G1-----G2    G = Grid point
 *     |     |     P = Query point (outside)
 *     |     |     E = Extrapolation limit
 *    G3--E--P     E = max(gridSpacing * extrapolationFactor)
 *    @endcode
 *
 * 3. Coverage Scoring
 *    For any point (lat, lon), we classify coverage as:
 *    - Full (1.0): Point within grid cells
 *    - Partial (0.0-1.0): Point within extrapolation distance
 *    - None (0.0): Point too far from grid
 *
 *    The coverage score for an area is:
 *    @code
 *    score = coveredPoints / totalPoints
 *    where coveredPoints includes both full and partial coverage
 *    weighted by their interpolation quality
 *    @endcode
 *
 * 4. Multiple Layer Coverage
 *    When layers overlap, we consider:
 *    a) Primary Coverage: Number of layers covering each point
 *       @code
 *       3 layers |   L1    |
 *       2 layers |   L1+L2 |
 *       1 layer  | L1+L2+L3|
 *       @endcode
 *
 *    b) Quality Metrics for each coverage:
 *       - Resolution of each layer
 *       - Distance to nearest grid points
 *       - Need for interpolation/extrapolation
 *
 * @see GribRecordSet for the structure of weather parameters at a time point
 * @see GRIBLayer for individual layer management
 *
 */
class LayerMergeStrategy {
public:
  /** Scoring method for comparing record options */
  enum class ScoringMethod {
    CURRENCY_FIRST,    //!< Prioritize most recent model runs
    RESOLUTION_FIRST,  //!< Prioritize highest resolution data
    MODEL_QUALITY,     //!< Use predefined model quality rankings
    USER_PRIORITY      //!< Strictly follow layer order
  };

  /**
   * Creates merge strategy with default settings.
   *
   * Default prioritizes data currency while respecting layer order
   * for tiebreaking.
   */
  LayerMergeStrategy() = default;

  /**
   * Sets the primary scoring method for record selection.
   *
   * @param method The scoring approach to use
   */
  void SetScoringMethod(ScoringMethod method);

  /**
   * Evaluates available records for a parameter at a time point.
   *
   * @param recordOptions Vector of available records from different layers
   * @param targetTime Time point being evaluated
   * @return Score for each record (higher is better)
   */
  std::vector<double> ScoreRecords(
      const std::vector<GribRecord*>& recordOptions, time_t targetTime) const;

  /**
   * Determines if records can be consistently merged.
   *
   * Checks for meteorological consistency across parameters.
   * @param selectedRecords Records chosen for merge
   * @return True if merge is valid
   */
  bool ValidateMerge(const std::vector<GribRecord*>& selectedRecords) const;

private:
  ScoringMethod m_scoringMethod;

  // Additional members to be added:
  // - Quality rankings for different models
  // - Resolution thresholds
  // - Currency scoring parameters
  // - Validation rules
};

#endif  // __GRIB_LAYER_MERGE_STRATEGY_H__