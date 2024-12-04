# Multi-Layer GRIB Support

## Overview

This document describes the implementation of multi-layer GRIB support in OpenCPN's GRIB plugin.
The changes allow users to load multiple GRIB files simultaneously and view them as overlapping layers.

## Key Features

- Support for up to 6 GRIB layers.
- Clear indication of which GRIB files are loaded into memory.
- Each layer can be enabled/disabled independently.
- Backward compatibility with existing weather routing plugin.

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
   - When multiple GRIB files provide data for the same parameter:
     - Uses data with best accuracy
     - Falls back to layer priority (lower index = higher priority)

3. **UI Changes**
   - Add layer control buttons.
   - Visual indicator showing enabled/disabled state.
   - Clear display of which files are loaded.

## Migration Path

The implementation follows a phased approach:

1. Initial Phase
   - Infrastructure for multiple layers (GribLayer and GribLayerManager).
   - Hard-code number of layers to 1.
   - Basic UI for layer management.
   - Maintain backward compatibility.

2. Future Phases
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
