# Multi-Layer GRIB Support

## Overview

This document describes the implementation of multi-layer GRIB support in OpenCPN's GRIB plugin.
The changes allow users to load multiple GRIB files simultaneously and view them as overlapping layers.

## Key Features

- Support for multiple GRIB layers. Each layer can load one GRIB file.
- Labels to indicate the names of the GRIBs for each layer.
- Each layer can be enabled/disabled independently.
- API to get GRIB data:
    - For a specific layer.
    - Across all GRIB layers, based on configured algorithm.

## Architecture Changes

### New Classes

#### GribLayer

A new class that encapsulates a single GRIB layer:

```c++
class GribLayer {
    bool m_enabled;           // Whether layer is enabled
    GRIBFile* m_pGribFile;    // The GRIB file for this layer
};
```

#### GribLayerManager

Manages multiple GRIB layers and provides data access:

```c++
class GribLayerManager {
    static const int MAX_LAYERS = 6;
    GribLayer* m_layers[MAX_LAYERS];
};
```

### Major Changes

1. **GRIBUICtrlBar Modifications**
   - Replace single `m_bGRIBActiveFile` with `GribLayerManager`.
   - Update file loading logic to work with layers.
   - Modify data access methods to get data from all enabled layers.

2. **Data Access Layer**
   - APIs to get GRIB data for a specified layer.
      - Use case: compute weather routing and compare results across different weather models.
   - APIs to get GRIB data across layers.
      - When multiple GRIB files provide data for the same parameter type:
         - Return data with best accuracy.
         - Falls back to layer priority (lower index = higher priority)

3. **UI Changes**
   - Add layer control buttons.
   - Visual indicator showing enabled/disabled state.
   - Clear display of which files are loaded.

## Migration Path

The implementation follows a phased approach:

1. Phase 1:
   - Add new classes for multiple GRIB layers: GribLayer and GribLayerManager.
   - Hard-code number of layers to 1.
   - No UI changes.
   - Replace `m_bGRIBActiveFile` with `m_pLayerManager`.
      - During the refactor work, keep both `m_pLayerManage` and `m_bGRIBActiveFile` while refactoring.
        Do one field/member function at a time and make sure at each iteration the code can compile and run without regressions.
      - Identify all the unique field/member access of `m_bGRIBActiveFile`.
      - Refactor all access to `m_bGRIBActiveFile` with `GetGribLayerManager()`, which should provides member functions.
      - The layer manager needs to expose functions that span all layers, and functions that are for specific layers.
      - For example, define `LayerManager.IsOK()`. This API could reasonably evolve to mean "at least one enabled layer is OK",
        since that would mean the manager has valid data to display.
        The signature can stay the same while the implementation changes.
      - `GetRefDateTime()` This is problematic because each layer could have a different reference time.
        `GetRefDateTime()` returning a single time doesn't make sense in a multi-layer context.
          - Change it to GetLayerRefDateTime(int layerIndex) to get a specific layer's time
          - Consider if we need a `GetEarliestRefDateTime()` or `GetLatestRefDateTime()` across all enabled layers

```c++
m_bGRIBActiveFile->IsOK
m_bGRIBActiveFile->m_GribIdxArray.Index(Idx_WIND_VX)
m_bGRIBActiveFile->GetRecordSetArrayPtr()
m_bGRIBActiveFile->GetRefDateTime()
m_bGRIBActiveFile->GetFileNames()
```

2. Phase 2:
   - Basic UI for layer management.
   - Maintain backward compatibility.

3. Phase 3:
   - Enhanced layer controls
   - Layer blending options
   - Extended data interpolation

## Backend Design Details

### Data Priority Rules

When multiple layers provide data for the same location:

1. Highest resolution/accuracy takes precedence
2. If equal resolution, highest priority layer wins
3. Time interpolation considers data from all enabled layers

### Backward Compatibility

- Existing code continues to work through compatibility methods
- Weather routing plugin still functions as before
- No changes required to existing GRIB file format

## Usage Examples

### Loading Multiple Files

```c++

// Example usage of layer manager
GribLayerManager* manager = new GribLayerManager();
manager->SetLayerFile(0, windFile);
manager->SetLayerFile(1, wavesFile);
manager->EnableLayer(0, true);
manager->EnableLayer(1, true);
```

### Getting Data

```c++
// Get data considering all enabled layers
double value = manager->GetTimeInterpolatedValue(
    recordType, lat, lon, time
);
```

## Configuration and Settings

The following settings are persisted:

- Enabled/disabled state of each layer
- Layer priorities
- File associations
