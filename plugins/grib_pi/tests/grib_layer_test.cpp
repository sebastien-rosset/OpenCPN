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

#include <wx/arrimpl.cpp>
#include <wx/dynarray.h>

#include "grib_layer.h"
#include "grib_file.h"
#include "mock_defs.h"

/**
 * Test fixture for GRIBLayer testing.
 *
 * Sets up an environment for testing GRIBLayer functionality.
 * Provides access to test files directory and common testing resources.
 */
class grib_layer_test : public ::testing::Test {
protected:
  void SetUp() override { testDataDir = wxString::FromUTF8(TESTDATA); }

  wxString testDataDir;
};

/**
 * Tests loading a valid GRIB file.
 *
 * Validates that a GRIBLayer can properly load and handle a valid GRIB file by:
 * - Loading an ECMWF GRIB2 file
 * - Verifying layer status and configuration
 * - Checking underlying file properties
 * - Validating record sets
 *
 */
TEST_F(grib_layer_test, LoadValidGribFile) {
  // Arrange
  wxArrayString fileNames;
  fileNames.Add(testDataDir + "/ocpn_ecmwf0p25_24_2024-11-24-18-29.grb2");

  // Act
  GRIBFile* file = new GRIBFile(fileNames, true, true);
  GRIBLayer layer("Test Layer", file, nullptr);

  // Assert
  EXPECT_TRUE(layer.IsOK())
      << "Layer should be valid after loading a correct GRIB file. "
      << "Last error: " << layer.GetLastError();
  EXPECT_TRUE(layer.IsEnabled()) << "Layer should be enabled by default";
  EXPECT_EQ("Test Layer", layer.GetName())
      << "Layer name should match what was set";
  EXPECT_TRUE(layer.GetLastError().IsEmpty())
      << "No error should be present. Got: " << layer.GetLastError();

  // Test underlying file
  ASSERT_TRUE(file->IsOK()) << "GRIB file should load successfully. "
                            << "Last message: " << file->GetLastMessage();
  EXPECT_FALSE(file->GetFileNames().IsEmpty())
      << "File names should not be empty";
  EXPECT_GT(file->GetCounter(), 0) << "File counter should be initialized";
  EXPECT_GT(file->GetRefDateTime(), 0) << "Reference date should be valid";

  const ArrayOfGribRecordSets* records = file->GetRecordSetArrayPtr();
  ASSERT_NE(records, nullptr) << "Record set array pointer should not be null";
  EXPECT_GT(records->GetCount(), 0)
      << "Should have at least one record set. "
      << "Got " << (records ? records->GetCount() : 0) << " records";
}

/**
 * Tests error handling for non-existent files.
 *
 * Validates GRIBLayer behavior when attempting to load a non-existent file:
 * - Verifies layer is marked as invalid
 * - Checks that layer remains enabled despite invalid file
 * - Ensures appropriate error messages are generated
 * - Validates file operation error handling
 */
TEST_F(grib_layer_test, NonExistentFile) {
  // Arrange
  wxArrayString fileNames;
  wxString testFile = testDataDir + "/does_not_exist.grb";
  fileNames.Add(testFile);

  // Act
  GRIBFile* file = new GRIBFile(fileNames, true, true);
  GRIBLayer layer("Test Layer", file, nullptr);

  // Assert
  EXPECT_FALSE(layer.IsOK())
      << "Layer should be invalid with non-existent file: " << testFile;
  EXPECT_TRUE(layer.IsEnabled()) << "Layer should still be enabled by default";
  EXPECT_FALSE(layer.GetLastError().IsEmpty())
      << "Error message should be present";

  // Test underlying file
  EXPECT_FALSE(file->IsOK()) << "GRIB file should fail to load";
  wxString lastMsg = file->GetLastMessage();
  EXPECT_TRUE(lastMsg.Contains("file") || lastMsg.Contains("open") ||
              lastMsg.Contains("exist"))
      << "Error message should mention file issue. Got: " << lastMsg;
}

/**
 * Tests handling of invalid/corrupt GRIB files.
 *
 * Validates GRIBLayer behavior when attempting to load a corrupted GRIB file:
 * - Verifies layer is marked as invalid
 * - Ensures layer remains enabled despite invalid content
 * - Checks for appropriate error messages
 * - Validates file parsing error handling
 */
TEST_F(grib_layer_test, InvalidGribFile) {
  // Arrange
  wxArrayString fileNames;
  wxString testFile = testDataDir + "/invalid.grb";
  fileNames.Add(testFile);

  // Act
  GRIBFile* file = new GRIBFile(fileNames, true, true);
  GRIBLayer layer("Test Layer", file, nullptr);

  // Assert
  EXPECT_FALSE(layer.IsOK())
      << "Layer should be invalid with corrupt GRIB file: " << testFile;
  EXPECT_TRUE(layer.IsEnabled()) << "Layer should still be enabled by default";
  EXPECT_FALSE(layer.GetLastError().IsEmpty())
      << "Error message should be present. Got: " << layer.GetLastError();

  // Test underlying file
  EXPECT_FALSE(file->IsOK()) << "GRIB file should fail to parse";
  wxString lastMsg = file->GetLastMessage();
  EXPECT_TRUE(file->GetLastMessage().Contains("can't be read!"))
      << "Error message should indicate parsing/format problem. Got: "
      << lastMsg;
}

/**
 * Tests layer enable/disable functionality.
 *
 * Validates that a GRIBLayer can be properly enabled and disabled:
 * - Verifies default enabled state
 * - Tests disable operation
 * - Tests re-enable operation
 * - Ensures state changes are persistent
 */
TEST_F(grib_layer_test, EnableDisableTest) {
  // Arrange
  wxArrayString fileNames;
  fileNames.Add(testDataDir + "/ocpn_ecmwf0p25_24_2024-11-24-18-29.grb2");
  GRIBFile* file = new GRIBFile(fileNames, true, true);
  GRIBLayer layer("Test Layer", file, nullptr);

  // Act & Assert
  EXPECT_TRUE(layer.IsEnabled()) << "Layer should be enabled by default";

  layer.SetEnabled(false);
  EXPECT_FALSE(layer.IsEnabled())
      << "Layer should be disabled after SetEnabled(false)";

  layer.SetEnabled(true);
  EXPECT_TRUE(layer.IsEnabled())
      << "Layer should be enabled after SetEnabled(true)";
}

/**
 * Tests file replacement functionality.
 *
 * Validates that a GRIBLayer can properly handle file replacement:
 * - Tests loading initial GRIB file
 * - Validates file replacement operation
 * - Verifies pointer management
 * - Ensures layer state is maintained
 * - Checks new file validity
 */
TEST_F(grib_layer_test, FileReplacementTest) {
  // Arrange
  wxArrayString fileNames;
  wxString testFile1 = testDataDir + "/ocpn_ecmwf0p25_24_2024-11-24-18-29.grb2";
  fileNames.Add(testFile1);
  GRIBFile* firstFile = new GRIBFile(fileNames, true, true);
  GRIBLayer layer("Test Layer", firstFile, nullptr);

  // Act - replace with new file
  wxArrayString newFileNames;
  wxString testFile2 =
      testDataDir + "/XyGrib_2025-01-20-12-43_GFS_0P25_WW3.grb2";
  newFileNames.Add(testFile2);
  GRIBFile* secondFile = new GRIBFile(newFileNames, true, true);

  // Store original file pointer for comparison
  const GRIBFile* originalFile = layer.GetFile();

  layer.SetGribFile(secondFile);

  // Assert
  EXPECT_NE(layer.GetFile(), originalFile)
      << "File pointer should change after replacement";
  EXPECT_EQ(layer.GetFile(), secondFile)
      << "File pointer should match new file";
  EXPECT_TRUE(layer.IsEnabled())
      << "Layer should remain enabled after file replacement";

  if (!secondFile->IsOK()) {
    ADD_FAILURE() << "Second test file failed to load: " << testFile2
                  << "\nError: " << secondFile->GetLastMessage();
  }
}
