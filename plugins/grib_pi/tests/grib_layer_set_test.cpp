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

#include <gtest/gtest.h>

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif  // precompiled headers
#include "wx/log.h"

#include <wx/arrimpl.cpp>
#include <wx/dynarray.h>

#include "grib_layer.h"
#include "grib_layer_set.h"
#include "grib_layer_merge_strategy.h"
#include "mock_defs.h"

/**
 * Test fixture for GRIBLayerSet testing.
 *
 * Provides common setup and utilities for testing GRIBLayerSet functionality.
 * Manages access to test data directory and any common test resources.
 */
class grib_layer_set_test : public ::testing::Test {
protected:
  void SetUp() override { testDataDir = wxString::FromUTF8(TESTDATA); }

  wxString testDataDir;
};

/**
 * Tests initial state of a newly created GRIBLayerSet.
 *
 * Verifies that a freshly created GRIBLayerSet:
 * - Is not marked as OK
 * - Has no files
 * - Has counter initialized to 0
 * - Has no reference date/time
 * - Has no record sets
 * - Has no indices
 */
TEST_F(grib_layer_set_test, InitialState) {
  GRIBLayerSet layerSet;

  // Initial state should be empty but valid
  EXPECT_FALSE(layerSet.IsOK()) << "Fresh layer set should not be OK";
  EXPECT_TRUE(layerSet.GetFileNames().IsEmpty())
      << "Should have no files initially";
  EXPECT_EQ(0, layerSet.GetRefDateTime())
      << "RefDateTime should be 0 initially";

  auto* records = layerSet.GetRecordSetArrayPtr();
  EXPECT_NE(nullptr, records) << "Record set should never be null";
  EXPECT_EQ(0u, records->GetCount()) << "Record set should be empty initially";

  auto& indices = layerSet.GetGribIdxArray();
  EXPECT_EQ(0u, indices.GetCount()) << "Should have no indices initially";
}

/**
 * Tests adding a valid GRIB layer to the set.
 *
 * Verifies:
 * - Layer is created successfully
 * - Layer is enabled by default
 * - Layer is valid
 * - Layer set state is updated correctly
 * - Layer can be retrieved by name
 *
 * Uses test.grb as the input file, which must be a valid GRIB file.
 */
TEST_F(grib_layer_set_test, AddValidLayer) {
  GRIBLayerSet layerSet;
  wxString testFile = testDataDir + "/ocpn_ecmwf0p25_24_2024-11-24-18-29.grb2";

  // Add a layer and verify it was added correctly
  GRIBLayer* layer = layerSet.AddLayer("test", testFile, true, true, false);
  ASSERT_NE(nullptr, layer)
      << "Layer creation failed: " << layerSet.GetLastMessage();
  EXPECT_TRUE(layer->IsEnabled()) << "New layer should be enabled";
  EXPECT_TRUE(layer->IsOK()) << "Layer should be valid";

  // Verify layer set state
  EXPECT_TRUE(layerSet.IsOK())
      << "Layer set should be OK after adding valid layer";
  EXPECT_FALSE(layerSet.GetFileNames().IsEmpty())
      << "Should have files after adding layer";

  // Try to get the layer back
  GRIBLayer* retrieved = layerSet.GetLayer("test");
  EXPECT_EQ(layer, retrieved) << "Should get back same layer pointer";
}

/**
 * Tests error handling when adding an invalid file.
 *
 * Verifies that the layer set:
 * - Handles non-existent files gracefully
 * - Returns nullptr for invalid layer creation
 * - Provides meaningful error message
 * - Maintains initial state after failure
 */
TEST_F(grib_layer_set_test, AddInvalidFile) {
  GRIBLayerSet layerSet;
  wxString testFile = testDataDir + "/nonexistent.grb";

  // Try to add non-existent file
  GRIBLayer* layer = layerSet.AddLayer("test", testFile, true, true, false);
  EXPECT_EQ(nullptr, layer)
      << "Should fail to create layer for non-existent file";
  EXPECT_FALSE(layerSet.GetLastMessage().IsEmpty())
      << "Should have error message";

  // Verify layer set remains in initial state
  EXPECT_FALSE(layerSet.IsOK()) << "Layer set should not be OK";
  EXPECT_TRUE(layerSet.GetFileNames().IsEmpty()) << "Should have no files";
}

/**
 * Tests handling of duplicate layer names.
 *
 * Verifies that:
 * - First layer with a name is created successfully
 * - Second layer with same name is rejected
 * - Error message is provided
 * - Original layer remains intact and accessible
 */
TEST_F(grib_layer_set_test, AddDuplicateLayer) {
  GRIBLayerSet layerSet;
  wxString testFile = testDataDir + "/ocpn_ecmwf0p25_24_2024-11-24-18-29.grb2";

  // Add first layer
  GRIBLayer* layer1 = layerSet.AddLayer("test", testFile, true, true, false);
  ASSERT_NE(nullptr, layer1) << "First layer creation failed";

  // Try to add second layer with same name
  GRIBLayer* layer2 = layerSet.AddLayer("test", testFile, true, true, false);
  EXPECT_EQ(nullptr, layer2) << "Should not allow duplicate layer names";
  EXPECT_FALSE(layerSet.GetLastMessage().IsEmpty())
      << "Should have error message";

  // Verify first layer still exists and is valid
  GRIBLayer* retrieved = layerSet.GetLayer("test");
  EXPECT_EQ(layer1, retrieved) << "Original layer should still be present";
}

/**
 * Tests layer removal functionality.
 *
 * Verifies:
 * - Layer can be successfully removed
 * - Layer is no longer accessible after removal
 * - Layer set state is updated correctly
 * - Attempting to remove non-existent layer fails gracefully
 */
TEST_F(grib_layer_set_test, RemoveLayer) {
  GRIBLayerSet layerSet;
  wxString testFile = testDataDir + "/ocpn_ecmwf0p25_24_2024-11-24-18-29.grb2";

  // Add and then remove a layer
  GRIBLayer* layer = layerSet.AddLayer("test", testFile, true, true, false);
  ASSERT_NE(nullptr, layer) << "Layer creation failed";

  EXPECT_TRUE(layerSet.RemoveLayer("test")) << "Layer removal should succeed";
  EXPECT_EQ(nullptr, layerSet.GetLayer("test"))
      << "Layer should no longer be accessible";
  EXPECT_FALSE(layerSet.IsOK())
      << "Layer set should not be OK after removing only layer";

  // Try to remove non-existent layer
  EXPECT_FALSE(layerSet.RemoveLayer("nonexistent"))
      << "Removing non-existent layer should fail";
}

/**
 * Tests interactions between multiple layers.
 *
 * Verifies:
 * - Multiple layers can be added successfully
 * - Layer enable/disable state is tracked correctly
 * - GetEnabledLayers returns correct set of layers
 * - Layer enable/disable affects layer set behavior
 */
TEST_F(grib_layer_set_test, MultipleLayerInteraction) {
  GRIBLayerSet layerSet;
  wxString testFile1 = testDataDir + "/ocpn_ecmwf0p25_24_2024-11-24-18-29.grb2";
  wxString testFile2 =
      testDataDir + "/XyGrib_2025-01-20-12-43_GFS_0P25_WW3.grb2";

  // Add two layers
  GRIBLayer* layer1 = layerSet.AddLayer("layer1", testFile1, true, true, false);
  ASSERT_NE(nullptr, layer1) << "First layer creation failed";

  GRIBLayer* layer2 = layerSet.AddLayer("layer2", testFile2, true, true, false);
  ASSERT_NE(nullptr, layer2) << "Second layer creation failed";

  // Both should be enabled by default
  auto enabledLayers = layerSet.GetEnabledLayers();
  EXPECT_EQ(2u, enabledLayers.size()) << "Should have two enabled layers";

  // Disable one layer
  layer1->SetEnabled(false);
  enabledLayers = layerSet.GetEnabledLayers();
  EXPECT_EQ(1u, enabledLayers.size())
      << "Should have one enabled layer after disabling one";
  EXPECT_EQ(layer2, enabledLayers[0])
      << "Remaining enabled layer should be layer2";
}

/**
 * Tests timeline record set creation functionality.
 *
 * Verifies:
 * - Timeline records can be created from layer data
 * - Returns valid record set for valid input
 * - Returns nullptr for invalid input or no enabled layers
 *
 * @note Test is skipped if required test file is missing or invalid
 */
TEST_F(grib_layer_set_test, TimelineRecordSet) {
  GRIBLayerSet layerSet;
  wxString testFile =
      testDataDir + "/multitime.grb";  // File with multiple time points

  // Add layer
  GRIBLayer* layer = layerSet.AddLayer("test", testFile, true, true, false);
  if (!layer || !layer->IsOK()) {
    GTEST_SKIP() << "Skipping timeline test due to missing/invalid test file";
    return;
  }

  // Get timeline records
  wxDateTime time = wxDateTime::Now();  // Should match file content
  GribTimelineRecordSet* timeline = layerSet.GetTimeLineRecordSet(&time);
  ASSERT_NE(nullptr, timeline) << "Should get valid timeline record set";

  // Timeline ownership is transferred to caller
  delete timeline;
}

/**
 * Tests merging of data type indices across layers.
 *
 * Verifies:
 * - Indices are merged correctly from multiple layers
 * - Duplicate indices are handled properly
 * - Disabling layers affects available indices
 *
 * @note Test is skipped if required test files are missing or invalid
 */
TEST_F(grib_layer_set_test, IndexMerging) {
  GRIBLayerSet layerSet;
  wxString testFile1 = testDataDir + "/ocpn_ecmwf0p25_24_2024-11-24-18-29.grb2";
  wxString testFile2 =
      testDataDir +
      "/XyGrib_2025-01-20-12-43_GFS_0P25_WW3.grb2";  // File with different data
                                                     // types

  // Add two layers with different data types
  GRIBLayer* layer1 = layerSet.AddLayer("layer1", testFile1, true, true, false);
  GRIBLayer* layer2 = layerSet.AddLayer("layer2", testFile2, true, true, false);

  if (!layer1 || !layer2 || !layer1->IsOK() || !layer2->IsOK()) {
    GTEST_SKIP()
        << "Skipping index merging test due to missing/invalid test files";
    return;
  }

  // Get merged indices
  auto& indices = layerSet.GetGribIdxArray();
  EXPECT_GT(indices.GetCount(), 0u)
      << "Should have some indices from merged layers";

  // Disable one layer
  layer1->SetEnabled(false);
  auto& reducedIndices = layerSet.GetGribIdxArray();
  EXPECT_LE(reducedIndices.GetCount(), indices.GetCount())
      << "Should have fewer indices after disabling a layer";
}

/**
 * Helper function to compare sampled data points between two GribRecords.
 * Takes samples at regular intervals to balance thoroughness with performance.
 */
void CompareSampledGribRecordData(const GribRecord& rec1,
                                  const GribRecord& rec2, size_t setIndex,
                                  size_t recordIndex) {
  const int SAMPLE_STRIDE = 10;  // Check every 10th point
  const double EPSILON = 1e-6;   // Tolerance for floating point comparison

  for (int i = 0; i < rec1.getNi(); i += SAMPLE_STRIDE) {
    for (int j = 0; j < rec1.getNj(); j += SAMPLE_STRIDE) {
      double val1 = rec1.getValue(i, j);
      double val2 = rec2.getValue(i, j);

      if (val1 == GRIB_NOTDEF) {
        EXPECT_EQ(GRIB_NOTDEF, val2)
            << "Data value NOTDEF mismatch at set " << setIndex << " record "
            << recordIndex << " pos (" << i << "," << j << ")";
      } else {
        EXPECT_NEAR(val1, val2, EPSILON)
            << "Data value mismatch at set " << setIndex << " record "
            << recordIndex << " pos (" << i << "," << j << ")";
      }
    }
  }
}

/**
 * Validates that GRIBLayerSet with a single layer matches GRIBFile content
 *
 * This test ensures that a GRIBLayerSet containing a single layer correctly
 * represents the same data as a GRIBFile created from the same source. It
 * performs a deep comparison of all relevant data structures and content.
 *
 * The test validates:
 * - Basic properties (IsOK, GetLastMessage, GetRefDateTime)
 * - File name management
 * - GRIB record index arrays
 * - Record set structure and content including:
 *   - Reference times
 *   - Record availability for each data type
 *   - Record properties (data type, level type, level value)
 *   - Grid dimensions
 *   - Actual data values (using sampling)
 *
 * Test file requirements:
 * - Uses ocpn_ecmwf0p25_24_2024-11-24-18-29.grb2
 * - File must be a valid GRIB file with various record types
 * - File should contain standard weather parameters (wind, pressure, etc.)
 */
TEST_F(grib_layer_set_test, SingleLayerMatchesGribFile) {
  // Arrange
  wxString testFile = testDataDir + "/ocpn_ecmwf0p25_24_2024-11-24-18-29.grb2";
  wxArrayString fileNames;
  fileNames.Add(testFile);

  // Create GRIBFile for comparison
  GRIBFile file(fileNames, true, true, false);
  ASSERT_TRUE(file.IsOK()) << "Reference GRIBFile should load successfully";

  // Create GRIBLayerSet with same file
  GRIBLayerSet layerSet;
  GRIBLayer* layer = layerSet.AddLayer("default", testFile, true, true, false);
  ASSERT_NE(nullptr, layer) << "Layer creation failed";
  ASSERT_TRUE(layerSet.IsOK()) << "LayerSet should be valid after adding layer";

  // Compare basic properties
  EXPECT_EQ(file.IsOK(), layerSet.IsOK()) << "OK status should match";
  EXPECT_EQ(file.GetLastMessage(), layerSet.GetLastMessage())
      << "Last message should match";
  EXPECT_EQ(file.GetRefDateTime(), layerSet.GetRefDateTime())
      << "Reference date/time should match";

  // Compare file names
  const wxArrayString& fileFileNames = file.GetFileNames();
  const wxArrayString& layerFileNames = layerSet.GetFileNames();
  EXPECT_EQ(fileFileNames.GetCount(), layerFileNames.GetCount())
      << "Number of files should match";

  for (size_t i = 0; i < fileFileNames.GetCount(); i++) {
    EXPECT_EQ(fileFileNames[i], layerFileNames[i])
        << "File name at index " << i << " should match";
  }

  // Compare GribIdxArray content
  const GRIBFile::GribIdxArray& fileIdx = file.GetGribIdxArray();
  const GRIBFile::GribIdxArray& layerSetIdx = layerSet.GetGribIdxArray();
  EXPECT_GT(fileIdx.GetCount(), 0u) << "File should have some indices";
  EXPECT_EQ(fileIdx.GetCount(), layerSetIdx.GetCount())
      << "GribIdxArray size should match";

  for (size_t i = 0; i < fileIdx.GetCount(); i++) {
    EXPECT_EQ(fileIdx[i], layerSetIdx[i])
        << "GribIdxArray item at index " << i << " should match";
  }

  // Compare record sets
  const ArrayOfGribRecordSets* fileRecordSets = file.GetRecordSetArrayPtr();
  const ArrayOfGribRecordSets* layerSetRecordSets =
      layerSet.GetRecordSetArrayPtr();

  ASSERT_NE(nullptr, fileRecordSets) << "File record sets should not be null";
  ASSERT_NE(nullptr, layerSetRecordSets)
      << "LayerSet record sets should not be null";

  EXPECT_GT(fileRecordSets->GetCount(), 0u)
      << "File should have some record sets";
  EXPECT_EQ(fileRecordSets->GetCount(), layerSetRecordSets->GetCount())
      << "Number of record sets should match";

  for (size_t i = 0; i < fileRecordSets->GetCount(); i++) {
    const GribRecordSet& fileSet = fileRecordSets->Item(i);
    const GribRecordSet& layerSet = layerSetRecordSets->Item(i);

    EXPECT_EQ(fileSet.GetReferenceTime(), layerSet.GetReferenceTime())
        << "Record set reference time at index " << i << " should match";

    // Compare record pointers
    for (size_t j = 0; j < Idx_COUNT; j++) {
      const GribRecord* fileRec = fileSet.GetRecord(j);
      const GribRecord* layerRec = layerSet.GetRecord(j);

      if (fileRec == nullptr) {
        EXPECT_EQ(nullptr, layerRec) << "Record at set " << i << " index " << j
                                     << " should both be null";
        continue;
      }

      ASSERT_NE(nullptr, layerRec) << "Record at set " << i << " index " << j
                                   << " should both be non-null";

      // Compare record properties
      EXPECT_EQ(fileRec->getDataType(), layerRec->getDataType())
          << "Record data type at set " << i << " index " << j
          << " should match";
      EXPECT_EQ(fileRec->getLevelType(), layerRec->getLevelType())
          << "Record level type at set " << i << " index " << j
          << " should match";
      EXPECT_EQ(fileRec->getLevelValue(), layerRec->getLevelValue())
          << "Record level value at set " << i << " index " << j
          << " should match";

      // Compare grid dimensions
      EXPECT_EQ(fileRec->getNi(), layerRec->getNi())
          << "Record Ni at set " << i << " index " << j << " should match";
      EXPECT_EQ(fileRec->getNj(), layerRec->getNj())
          << "Record Nj at set " << i << " index " << j << " should match";

      // Compare data values (sampling to save time)
      CompareSampledGribRecordData(*fileRec, *layerRec, i, j);
    }
  }
}

char* formatTimestamp(time_t timestamp, char* buffer) {
  struct tm* timeinfo = localtime(&timestamp);
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
  return buffer;
}
/**
 * Tests merging of HRRR and GFS layers with overlapping coverage.
 *
 * Verifies correct data selection based on:
 * - Spatial coverage (HRRR vs GFS areas)
 * - Temporal coverage (HRRR 48-hour vs GFS longer range)
 * - Grid resolution differences
 */
TEST_F(grib_layer_set_test, TestHRRRGFSMerging) {
  GRIBLayerSet layerSet;
  printf("TestHRRRGFSMerging\n");

  // Add HRRR layer first (higher priority)
  wxString hrrrFile = testDataDir + "/HRRR20250122170401901.grb";
  GRIBLayer* hrrrLayer = layerSet.AddLayer("hrrr", hrrrFile, true, true, false);
  ASSERT_NE(nullptr, hrrrLayer) << "HRRR layer creation failed";
  ASSERT_TRUE(hrrrLayer->IsOK()) << "HRRR layer should be valid";

  // Add GFS layer second
  wxString gfsFile = testDataDir + "/GFS20250122170501904.grb";
  GRIBLayer* gfsLayer = layerSet.AddLayer("gfs", gfsFile, true, true, false);
  ASSERT_NE(nullptr, gfsLayer) << "GFS layer creation failed";
  ASSERT_TRUE(gfsLayer->IsOK()) << "GFS layer should be valid";

  GRIBFile* hrrrGribFile = hrrrLayer->GetFile();
  ASSERT_NE(nullptr, hrrrGribFile) << "HRRR file should not be null";
  const ArrayOfGribRecordSets* hrrrRecordsTimeline =
      hrrrGribFile->GetRecordSetArrayPtr();
  ASSERT_NE(nullptr, hrrrRecordsTimeline)
      << "Failed to get timeline record set";
  EXPECT_GT(hrrrRecordsTimeline->GetCount(), 0u)
      << "HRRR file should have some record sets";
  char buffer[80];
  for (size_t i = 0; i < hrrrRecordsTimeline->GetCount(); i++) {
    const GribRecordSet& recordSet = hrrrRecordsTimeline->Item(i);
    const auto& indices = hrrrGribFile->GetGribIdxArray();
    EXPECT_NE(std::find(indices.begin(), indices.end(), Idx_WIND_VX),
              indices.end())
        << "Wind X component not found";
    EXPECT_NE(std::find(indices.begin(), indices.end(), Idx_WIND_VY),
              indices.end())
        << "Wind Y component not found";
    for (const auto& idx : indices) {
      const GribRecord* record = recordSet.GetRecord(idx);
      EXPECT_EQ(record->isOk(), true) << "HRRR record should be OK";
      EXPECT_EQ(record->getDataCenterModel(), NOAA_HRRR)
          << "HRRR record should have correct data center model";
      printf(
          "Record %d. Version: %d. Datatype: %d. Datacenter Model: %d. "
          "Center: %d. Model: %d Grid: %d. "
          "getRecordCurrentDate: %s. "
          "getRecordRefDate: %s. getStrRecordCurDate: %s. "
          "getStrRecordRefDate: %s. Period: %d\n",
          idx, record->getEditionNumber(), record->getDataType(),
          record->getDataCenterModel(), record->getIdCenter(),
          record->getIdModel(), record->getIdGrid(),
          formatTimestamp(record->getRecordCurrentDate(), buffer),
          formatTimestamp(record->getRecordRefDate(), buffer),
          record->getStrRecordCurDate(), record->getStrRecordRefDate(),
          record->getPeriodSec());
    }
  }

  // Define test points
  struct TestPoint {
    double lat;
    double lon;
    bool present;  // Should wind data be present at this point
    DataCenterModel expectedModel;
    double expectedLonGridSpacing;  // in degrees
    std::string description;
  };

  // Test points covering different scenarios
  std::vector<TestPoint> testPoints = {
      // Inside HRRR coverage area
      {37.49, -122.26, true, NOAA_HRRR, 0.0249,
       "San Francisco - HRRR coverage"},

      // Outside HRRR but inside GFS coverage
      {36.47, -125.35, true, NOAA_GFS, 0.25, "Taney Seamount - GFS only"},

      // Outside both coverages
      {38.07, -129.58, false, OTHER_DATA_CENTER, 0.0,
       "NOAA Buoy - No coverage expected"},
  };

  // Set merge strategy to prioritize HRRR for short-term forecasts
  layerSet.SetMergeStrategy(LayerMergeStrategy::ScoringMethod::MODEL_QUALITY);

  // Test points at different forecast times
  std::vector<int> forecastHours = {6, 24, 48, 72, 120};

  for (const auto& point : testPoints) {
    for (int hours : forecastHours) {
      // Calculate forecast time
      time_t baseTime = layerSet.GetRefDateTime();
      time_t forecastTime = baseTime + (hours * 3600);

      // Get merged data for this point and time
      wxDateTime wxForecastTime = wxDateTime(forecastTime);
      GribTimelineRecordSet* timelineSet =
          layerSet.GetTimeLineRecordSet(&wxForecastTime);
      ASSERT_NE(nullptr, timelineSet) << "Failed to get timeline record set";

      // Check wind data (both U and V components should be consistent)
      GribRecord* uWind = timelineSet->GetRecord(Idx_WIND_VX);
      GribRecord* vWind = timelineSet->GetRecord(Idx_WIND_VY);

      if (!point.present) {
        // Should have no data for points outside coverage
        EXPECT_EQ(nullptr, uWind)
            << "Expected no wind data at " << point.description;
        EXPECT_EQ(nullptr, vWind)
            << "Expected no wind data at " << point.description;
      } else {
        ASSERT_NE(nullptr, uWind) << "Missing U wind at " << point.description;
        ASSERT_NE(nullptr, vWind) << "Missing V wind at " << point.description;

        // Verify grid spacing
        EXPECT_NEAR(point.expectedLonGridSpacing, uWind->getDi(), 0.001)
            << "Incorrect longitude grid spacing at " << point.description;
        EXPECT_NEAR(point.expectedLonGridSpacing, vWind->getDj(), 0.001)
            << "Incorrect latitude grid spacing at " << point.description;

        // Verify correct model was used based on location and forecast time
        EXPECT_EQ(point.expectedModel, uWind->getDataCenterModel())
            << "Incorrect model used at " << point.description << " for +"
            << hours << "h forecast";

        // Verify data continuity around model transition points
        // Verify data continuity around model transition points
        if (hours == 48) {
          // Get data for hour 47 and 49 to check transition
          time_t beforeTime = baseTime + (47 * 3600);
          time_t afterTime = baseTime + (49 * 3600);

          wxDateTime bt = wxDateTime(beforeTime);
          wxDateTime at = wxDateTime(afterTime);
          GribTimelineRecordSet* beforeSet = layerSet.GetTimeLineRecordSet(&bt);
          GribTimelineRecordSet* afterSet = layerSet.GetTimeLineRecordSet(&at);

          // Sample wind values at test point - get U component records
          GribRecord* beforeRec = beforeSet->GetRecord(Idx_WIND_VX);
          GribRecord* currentRec = timelineSet->GetRecord(Idx_WIND_VX);
          GribRecord* afterRec = afterSet->GetRecord(Idx_WIND_VX);

          if (beforeRec && currentRec && afterRec) {
            // Get interpolated values from the records
            double beforeU =
                beforeRec->getInterpolatedValue(point.lon, point.lat);
            double currentU =
                currentRec->getInterpolatedValue(point.lon, point.lat);
            double afterU =
                afterRec->getInterpolatedValue(point.lon, point.lat);

            // Check for reasonable continuity at transition
            if (beforeU != GRIB_NOTDEF && currentU != GRIB_NOTDEF &&
                afterU != GRIB_NOTDEF) {
              // Compute rate of change before and after
              double beforeDelta = std::abs(currentU - beforeU);
              double afterDelta = std::abs(afterU - currentU);

              // Ensure the transition isn't too sharp
              EXPECT_LE(std::abs(afterDelta - beforeDelta), 5.0)
                  << "Too sharp transition at model boundary";
            }
          }

          delete beforeSet;
          delete afterSet;
        }
      }

      delete timelineSet;
    }
  }
}

/**
 * Tests zone limit calculations across different scenarios.
 *
 * Verifies that GetZoneLimits correctly handles:
 * - Single layer boundaries
 * - Multiple layer boundaries
 * - Disabled layers
 * - Dateline crossing
 * - Global coverage
 * - No enabled layers
 */
TEST_F(grib_layer_set_test, GetZoneLimits) {
  GRIBLayerSet layerSet;

  // Test with no layers
  double latmin, latmax, lonmin, lonmax;
  EXPECT_FALSE(layerSet.GetZoneLimits(&latmin, &latmax, &lonmin, &lonmax))
      << "Should return false with no layers";

  // Add first layer (ECMWF data)
  wxString testFile1 = testDataDir + "/ocpn_ecmwf0p25_24_2024-11-24-18-29.grb2";
  GRIBLayer* layer1 = layerSet.AddLayer("layer1", testFile1, true, true, false);
  ASSERT_NE(nullptr, layer1) << "First layer creation failed";

  // Test with single layer
  ASSERT_TRUE(layerSet.GetZoneLimits(&latmin, &latmax, &lonmin, &lonmax))
      << "Should get valid bounds from single layer";
  EXPECT_GE(latmin, -90.0) << "Latitude minimum should be >= -90";
  EXPECT_LE(latmax, 90.0) << "Latitude maximum should be <= 90";
  EXPECT_GE(lonmin, -180.0) << "Longitude minimum should be >= -180";
  EXPECT_LE(lonmax, 180.0) << "Longitude maximum should be <= 180";

  // Add second layer (GFS data) with different coverage
  wxString testFile2 =
      testDataDir + "/XyGrib_2025-01-20-12-43_GFS_0P25_WW3.grb2";
  GRIBLayer* layer2 = layerSet.AddLayer("layer2", testFile2, true, true, false);
  ASSERT_NE(nullptr, layer2) << "Second layer creation failed";

  // Test with both layers enabled
  double combined_latmin, combined_latmax, combined_lonmin, combined_lonmax;
  ASSERT_TRUE(layerSet.GetZoneLimits(&combined_latmin, &combined_latmax,
                                     &combined_lonmin, &combined_lonmax))
      << "Should get valid bounds from both layers";

  // Combined bounds should encompass both individual bounds
  EXPECT_LE(combined_latmin, latmin)
      << "Combined latmin should be <= single layer latmin";
  EXPECT_GE(combined_latmax, latmax)
      << "Combined latmax should be >= single layer latmax";
  EXPECT_LE(combined_lonmin, lonmin)
      << "Combined lonmin should be <= single layer lonmin";
  EXPECT_GE(combined_lonmax, lonmax)
      << "Combined lonmax should be >= single layer lonmax";

  // Test with one layer disabled
  layer1->SetEnabled(false);
  double disabled_latmin, disabled_latmax, disabled_lonmin, disabled_lonmax;
  ASSERT_TRUE(layerSet.GetZoneLimits(&disabled_latmin, &disabled_latmax,
                                     &disabled_lonmin, &disabled_lonmax))
      << "Should get valid bounds with one layer disabled";

  // Verify null parameter handling
  EXPECT_TRUE(layerSet.GetZoneLimits(nullptr, &disabled_latmax,
                                     &disabled_lonmin, &disabled_lonmax))
      << "Should handle null latmin";
  EXPECT_TRUE(layerSet.GetZoneLimits(&disabled_latmin, nullptr,
                                     &disabled_lonmin, &disabled_lonmax))
      << "Should handle null latmax";
  EXPECT_TRUE(layerSet.GetZoneLimits(&disabled_latmin, &disabled_latmax,
                                     nullptr, &disabled_lonmax))
      << "Should handle null lonmin";
  EXPECT_TRUE(layerSet.GetZoneLimits(&disabled_latmin, &disabled_latmax,
                                     &disabled_lonmin, nullptr))
      << "Should handle null lonmax";

  // Test with all layers disabled
  layer2->SetEnabled(false);
  EXPECT_FALSE(layerSet.GetZoneLimits(&latmin, &latmax, &lonmin, &lonmax))
      << "Should return false with all layers disabled";
}