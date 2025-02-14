/**********************************************************************
zyGrib: meteorological GRIB file viewer
Copyright (C) 2008 - Jacques Zaninetti - http://www.zygrib.org

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
***********************************************************************/
/**
 * \file
 * GRIB Version 2 Record Implementation
 *
 * Implements record handling for the GRIB2 format, which is the modern standard
 * for meteorological data distribution. GRIB2 provides enhanced capabilities
 * and flexibility compared to GRIB1, such as support for variable-length
 * sections, data compression, ensemble forecast data and more coordinate system
 * options.
 */
#ifndef GRIBV2RECORD_H
#define GRIBV2RECORD_H

#include <iostream>
#include <cmath>

#include "zuFile.h"
#include "GribRecord.h"

class GRIBMessage;

//----------------------------------------------
class GribV2Record : public GribRecord {
public:
  GribV2Record(ZUFILE* file, int id_);
  GribV2Record(const GribRecord& rec);
  GribV2Record() { grib_msg = 0; }

  ~GribV2Record();

  // return a new record for next data set
  GribV2Record* GribV2NextDataSet(ZUFILE* file, int id_);
  bool hasMoreDataSet() const;

private:
  zuint periodSeconds(zuchar unit, zuint P1, zuint P2, zuchar range);
  void readDataSet(ZUFILE* file);
  class GRIBMessage* grib_msg;
  /**
   * Maps raw GRIB parameters to standardized OpenCPN data types and handles
   * model-specific adjustments.
   *
   * 1. Identify the source model/center based on GRIB header values and set
   *    dataCenterModel accordingly
   *
   * 2. Adjust data types and level values based on model-specific requirements:
   *    - Map parameter codes to GRB_* data types
   *    - Convert units if needed (e.g., mm/s to mm/h for precipitation)
   *    - Standardize level types and values
   *    - Handle model-specific parameter encodings
   *
   * The function modifies these member variables:
   * - dataCenterModel: Set to identified model from DataCenterModel enum
   * - knownData: Set false if data type/model cannot be identified
   * - dataType: Mapped to standardized GRB_* parameter code
   * - levelType: Standardized vertical level type
   * - levelValue: Adjusted level value (e.g., Pa to hPa conversion)
   *
   * @note This function is called during record construction after the raw GRIB
   *       data has been read but before the record is used.
   */
  void translateDataType();

  bool mapTimeRange(GRIBMessage* grid, zuint* p1, zuint* p2, zuchar* t_range,
                    int* n_avg, int* n_missing, int center) const;

  //---------------------------------------------
  // SECTION 0: THE INDICATOR SECTION (IS)
  //---------------------------------------------
  zuint fileOffset0;
  zuint seekStart, totalSize;
  // zuchar editionNumber;
  bool b_len_add_8;

  // SECTION 1: THE PRODUCT DEFINITION SECTION (PDS)
  zuint fileOffset1;
  zuint sectionSize1;
  zuchar tableVersion;
  zuchar data1[28];
  bool hasGDS;
  // bool   hasBMS;
  double decimalFactorD;
  // SECTION 2: THE GRID DESCRIPTION SECTION (GDS)
  zuint fileOffset2;
  zuint sectionSize2;
  // SECTION 3: BIT MAP SECTION (BMS)
  zuint fileOffset3;
  zuint sectionSize3;
  // zuchar *BMSbits;
  // SECTION 4: BINARY DATA SECTION (BDS)
  int productTemplate;
  int productDiscipline;
  int gridTemplateNum;
  int dataCat;
  int dataNum;

  zuint fileOffset4;
  zuint sectionSize4;
  zuchar unusedBitsEndBDS;
  bool isGridData;  // not spherical harmonics
  bool isSimplePacking;
  bool isFloatValues;
  int scaleFactorE;
  double scaleFactorEpow2;
  double refValue;
  zuint nbBitsInPack;
  // SECTION 5: END SECTION (ES)

  //---------------------------------------------
  // Data Access
  //---------------------------------------------
  bool readGribSection0_IS(ZUFILE* file, bool b_skip_initial_GRIB);
};

#endif
