# Task 1.2: Design Multi-Source Spatial Feature Abstraction

**Phase**: Foundation & Analysis  
**Dependencies**: Task 1.1

## Detailed Description

Design a unified abstraction layer that can extract and represent spatial features from any available data source in a user's OpenCPN installation, enabling consistent spatial indexing regardless of the underlying data format.

## Abstraction Design Goals

### 1. Universal Feature Representation

- Design common feature types that can represent coastlines, depth contours, hazards, and navigation aids from any source
- Create abstraction for different geometry types (polygons, polylines, points) used across data systems
- Handle feature metadata and attributes consistently across different source formats
- Support hierarchical feature relationships (e.g., land/lake/island from GSHHS, chart object classes from S-57)

#### Route Safety Configuration - S-57 ENC Object Mappings

We may want to extract the following ENC features:

- Safety Depth → DEPARE, DEPCNT
- Safety Height → BRIDGE, CBLOHD
- Depths Above Safety Depth From 3D Database → DEPARE, SOUNDG
- Active Captain Hazard Points → OBSTRN
- Alarm Areas → PRCARE, RESARE
- Obstructions Above Safety Depth → OBSTRN
- Underwater and Awash Rock Above Safety Depth → UWTROC
- Fishing Facilities Above Safety Depth → FISINS
- Depth Areas Above Safety Depth → DEPARE
- Land Area → LNDARE
- Shoreline Constructions → SLCONS
- Depth Soundings Above Safety Depth → SOUNDG
- Bridges and Cables Clearances → BRIDGE, CBLOHD
- Marine Farm → MARCUL
- Traffic Separation Zone → TSSRON
- Inshore Traffic Zone → ISTZNE
- Restricted Area → RESARE
- Precautionary Area → PRCARE
- Offshore Platform → OFSPLF
- Military Zone → MIPARE
- Landing Area → LNDRGN
- Sub Marine Transit Lane → SUBTLN
- Mooring Zone → MORFAC
- PSSA Zone → PSSARE

### 2. Dynamic Data Source Discovery

- Design system to automatically detect available spatial data sources in user installations
- Create priority and quality assessment framework for overlapping data sources
- Handle graceful degradation when preferred data sources are unavailable
- Support runtime addition of new data sources (e.g., user-installed charts or plugins)

### 3. Extensible Adapter Framework

- Design plugin-like architecture for data source adapters
- Create common interface for feature extraction from different formats
- Support streaming extraction for large datasets
- Handle coordinate system transformations and datum conversions uniformly

## Integration Considerations

- Build upon existing data loading patterns and caching mechanisms
- Maintain compatibility with current plugin APIs and data access methods
- Leverage existing coordinate transformation and geometric utility functions
- Ensure minimal impact on current chart loading and rendering performance

## Acceptance Criteria

- [x] Unified feature representation that can handle all identified data sources
- [x] Extensible adapter framework design that supports future data formats  
- [x] Dynamic data source discovery and prioritization strategy
- [x] Clear integration path with existing OpenCPN data systems
- [x] Performance model that doesn't degrade existing functionality

## Design Documents

### Core Architecture Overview

Based on Task 1.1 analysis, the multi-source spatial abstraction must handle five distinct data systems:

1. **GSHHS** - Global polygon coastlines (5 quality levels)
2. **OSMSHP** - OpenStreetMap shapefile coastlines (5 quality levels) 
3. **S-57/ENC** - Professional vector charts (100+ object classes)
4. **Shapefile Basemaps** - Generic shapefile features
5. **Plugin Data Sources** - Third-party spatial data

### Unified Feature Model

```cpp
// Core spatial feature abstraction
class SpatialFeature {
public:
    enum class FeatureType {
        COASTLINE,           // Land/water boundary
        DEPTH_CONTOUR,       // Depth lines and areas
        NAVIGATION_HAZARD,   // Rocks, wrecks, obstructions
        NAVIGATION_AID,      // Lights, buoys, beacons
        RESTRICTED_AREA,     // Traffic zones, military areas
        INFRASTRUCTURE      // Bridges, cables, platforms
    };
    
    enum class GeometryType {
        POINT,              // Single coordinate
        POLYLINE,           // Line string
        POLYGON,            // Closed area
        MULTIPOLYGON        // Complex areas with holes
    };
    
    virtual ~SpatialFeature() = default;
    virtual FeatureType GetFeatureType() const = 0;
    virtual GeometryType GetGeometryType() const = 0;
    virtual LLBBox GetBoundingBox() const = 0;
    virtual bool IntersectsLine(const wxLineF& line) const = 0;
    virtual double GetMinimumDistance(const wxRealPoint& point) const = 0;
    
    // Data source information
    virtual std::string GetDataSource() const = 0;
    virtual int GetQualityLevel() const = 0;
    virtual double GetDataAccuracy() const = 0;
    
    // Feature-specific attributes
    virtual std::map<std::string, std::any> GetAttributes() const = 0;
};

// Specialized feature types
class CoastlineFeature : public SpatialFeature {
private:
    std::vector<wxRealPoint> m_vertices;
    int m_gshhsLevel; // 0=land, 1=lake, 2=island_in_lake, etc.
    bool m_isLand;    // true=land, false=water
    
public:
    FeatureType GetFeatureType() const override { return FeatureType::COASTLINE; }
    bool IsLandBoundary() const { return m_isLand; }
    int GetGshhsHierarchy() const { return m_gshhsLevel; }
    const std::vector<wxRealPoint>& GetVertices() const { return m_vertices; }
};

class DepthFeature : public SpatialFeature {
private:
    double m_depth;           // Depth in meters
    bool m_isSafetyDepth;    // Is this a safety contour?
    std::vector<wxRealPoint> m_vertices;
    
public:
    FeatureType GetFeatureType() const override { return FeatureType::DEPTH_CONTOUR; }
    double GetDepth() const { return m_depth; }
    bool IsSafetyDepth() const { return m_isSafetyDepth; }
    const std::vector<wxRealPoint>& GetVertices() const { return m_vertices; }
};

class HazardFeature : public SpatialFeature {
private:
    wxRealPoint m_position;
    double m_depth;          // Depth of hazard (-1 if unknown)
    std::string m_hazardType; // Rock, wreck, obstruction, etc.
    
public:
    FeatureType GetFeatureType() const override { return FeatureType::NAVIGATION_HAZARD; }
    GeometryType GetGeometryType() const override { return GeometryType::POINT; }
    const wxRealPoint& GetPosition() const { return m_position; }
    double GetHazardDepth() const { return m_depth; }
    const std::string& GetHazardType() const { return m_hazardType; }
};
```

### Data Source Discovery and Prioritization

```cpp
// Data source management
class SpatialDataSource {
public:
    enum class SourceType {
        GSHHS,
        OSMSHP,
        S57_ENC,
        SHAPEFILE,
        PLUGIN
    };
    
    virtual ~SpatialDataSource() = default;
    virtual SourceType GetSourceType() const = 0;
    virtual std::string GetSourceName() const = 0;
    virtual LLBBox GetCoverage() const = 0;
    virtual int GetQualityLevel() const = 0;
    virtual bool IsAvailable() const = 0;
    
    // Feature extraction interface
    virtual std::vector<std::unique_ptr<SpatialFeature>> 
        ExtractFeatures(const LLBBox& bounds, 
                        SpatialFeature::FeatureType type) const = 0;
};

// Priority-based data source manager
class DataSourceManager {
private:
    std::vector<std::unique_ptr<SpatialDataSource>> m_sources;
    std::map<LLBBox, std::vector<SpatialDataSource*>> m_spatialIndex;
    
public:
    // Automatic discovery of available data sources
    void DiscoverDataSources();
    
    // Get best available data source for a region and feature type
    SpatialDataSource* GetBestSource(const LLBBox& region, 
                                    SpatialFeature::FeatureType type) const;
    
    // Priority order: ENC > OSMSHP > GSHHS (where available)
    std::vector<SpatialDataSource*> GetPrioritizedSources(
        const LLBBox& region) const;
    
    // Register plugin data sources at runtime
    void RegisterPluginSource(std::unique_ptr<SpatialDataSource> source);
};
```

### Extensible Adapter Framework

```cpp
// Base adapter for different data formats
class SpatialDataAdapter {
public:
    virtual ~SpatialDataAdapter() = default;
    virtual bool CanHandle(const std::string& dataPath) const = 0;
    virtual std::unique_ptr<SpatialDataSource> CreateDataSource(
        const std::string& dataPath) const = 0;
};

// GSHHS adapter implementation
class GshhsAdapter : public SpatialDataAdapter {
public:
    bool CanHandle(const std::string& dataPath) const override {
        return dataPath.find("gshhs") != std::string::npos ||
               dataPath.find(".b") != std::string::npos;
    }
    
    std::unique_ptr<SpatialDataSource> CreateDataSource(
        const std::string& dataPath) const override {
        return std::make_unique<GshhsDataSource>(dataPath);
    }
};

// S-57 ENC adapter implementation  
class S57Adapter : public SpatialDataAdapter {
public:
    bool CanHandle(const std::string& dataPath) const override {
        return dataPath.find(".000") != std::string::npos ||
               dataPath.find("ENC_ROOT") != std::string::npos;
    }
    
    std::unique_ptr<SpatialDataSource> CreateDataSource(
        const std::string& dataPath) const override {
        return std::make_unique<S57DataSource>(dataPath);
    }
};

// OSMSHP adapter implementation
class OsmshpAdapter : public SpatialDataAdapter {
public:
    bool CanHandle(const std::string& dataPath) const override {
        return dataPath.find("osmshp") != std::string::npos;
    }
    
    std::unique_ptr<SpatialDataSource> CreateDataSource(
        const std::string& dataPath) const override {
        return std::make_unique<OsmshpDataSource>(dataPath);
    }
};

// Registry for all adapters
class AdapterRegistry {
private:
    std::vector<std::unique_ptr<SpatialDataAdapter>> m_adapters;
    
public:
    AdapterRegistry() {
        // Register built-in adapters
        m_adapters.push_back(std::make_unique<S57Adapter>());
        m_adapters.push_back(std::make_unique<OsmshpAdapter>());
        m_adapters.push_back(std::make_unique<GshhsAdapter>());
    }
    
    // Find appropriate adapter for data file
    SpatialDataAdapter* FindAdapter(const std::string& dataPath) const;
    
    // Allow plugins to register custom adapters
    void RegisterAdapter(std::unique_ptr<SpatialDataAdapter> adapter);
};
```

### S-57 ENC Object Mapping

```cpp
// S-57 specific feature extraction
class S57FeatureExtractor {
public:
    // Map S-57 object classes to our unified feature types
    static SpatialFeature::FeatureType MapS57ObjectClass(const std::string& objclass) {
        static const std::map<std::string, SpatialFeature::FeatureType> mappings = {
            // Coastline and land features
            {"LNDARE", SpatialFeature::FeatureType::COASTLINE},
            {"SLCONS", SpatialFeature::FeatureType::COASTLINE},
            
            // Depth features
            {"DEPARE", SpatialFeature::FeatureType::DEPTH_CONTOUR},
            {"DEPCNT", SpatialFeature::FeatureType::DEPTH_CONTOUR},
            {"SOUNDG", SpatialFeature::FeatureType::DEPTH_CONTOUR},
            
            // Hazards
            {"OBSTRN", SpatialFeature::FeatureType::NAVIGATION_HAZARD},
            {"UWTROC", SpatialFeature::FeatureType::NAVIGATION_HAZARD},
            {"WRECKS", SpatialFeature::FeatureType::NAVIGATION_HAZARD},
            
            // Infrastructure
            {"BRIDGE", SpatialFeature::FeatureType::INFRASTRUCTURE},
            {"CBLOHD", SpatialFeature::FeatureType::INFRASTRUCTURE},
            {"OFSPLF", SpatialFeature::FeatureType::INFRASTRUCTURE},
            
            // Restricted areas
            {"RESARE", SpatialFeature::FeatureType::RESTRICTED_AREA},
            {"PRCARE", SpatialFeature::FeatureType::RESTRICTED_AREA},
            {"TSSRON", SpatialFeature::FeatureType::RESTRICTED_AREA},
            {"ISTZNE", SpatialFeature::FeatureType::RESTRICTED_AREA},
            {"MIPARE", SpatialFeature::FeatureType::RESTRICTED_AREA}
        };
        
        auto it = mappings.find(objclass);
        return it != mappings.end() ? it->second : SpatialFeature::FeatureType::NAVIGATION_HAZARD;
    }
    
    // Extract safety-critical attributes from S-57 features
    static std::map<std::string, std::any> ExtractSafetyAttributes(
        const OGRFeature* s57Feature) {
        std::map<std::string, std::any> attributes;
        
        // Extract depth information
        if (s57Feature->GetFieldIndex("DRVAL1") >= 0) {
            attributes["depth_range_min"] = s57Feature->GetFieldAsDouble("DRVAL1");
        }
        if (s57Feature->GetFieldIndex("DRVAL2") >= 0) {
            attributes["depth_range_max"] = s57Feature->GetFieldAsDouble("DRVAL2");
        }
        
        // Extract clearance information for bridges/cables
        if (s57Feature->GetFieldIndex("VERCLR") >= 0) {
            attributes["vertical_clearance"] = s57Feature->GetFieldAsDouble("VERCLR");
        }
        
        // Extract restriction information
        if (s57Feature->GetFieldIndex("RESTRN") >= 0) {
            attributes["restriction_type"] = s57Feature->GetFieldAsInteger("RESTRN");
        }
        
        return attributes;
    }
};
```

## Implementation Notes

### Integration with Existing OpenCPN Systems

Based on Task 1.1 analysis, the abstraction layer must integrate carefully with existing systems:

#### 1. Backward Compatibility Strategy

**Maintain Current API**: The existing `PlugIn_GSHHS_CrossesLand()` function must continue to work unchanged. The new system will operate as a high-performance backend while preserving the same interface.

```cpp
// Existing API preserved
extern "C" bool PlugIn_GSHHS_CrossesLand(double lat1, double lon1, 
                                          double lat2, double lon2) {
    // Route through new unified system but maintain exact same behavior
    return g_UnifiedSpatialIndex->CrossesLand(lat1, lon1, lat2, lon2);
}
```

**Gradual Migration Path**: 
- Phase 1: New system operates alongside existing GSHHS system
- Phase 2: Plugins can opt-in to enhanced spatial API
- Phase 3: Full migration with performance monitoring

#### 2. Data Source Priority Logic

Based on Task 1.1 findings, implement intelligent data source selection:

```cpp
class DataSourcePriority {
public:
    // Priority order based on data quality analysis from Task 1.1
    static int GetPriority(SpatialDataSource::SourceType type, 
                          const LLBBox& region) {
        switch (type) {
            case SpatialDataSource::SourceType::S57_ENC:
                return 100;  // Highest - official hydrographic office data
            case SpatialDataSource::SourceType::OSMSHP:
                return 80;   // High - good OSM coverage areas
            case SpatialDataSource::SourceType::GSHHS:
                return 60;   // Medium - global fallback
            case SpatialDataSource::SourceType::SHAPEFILE:
                return 40;   // Lower - generic shapefiles
            case SpatialDataSource::SourceType::PLUGIN:
                return 20;   // Variable - depends on plugin quality
        }
        return 0;
    }
    
    // Regional adjustments based on known data quality
    static int AdjustForRegion(int basePriority, 
                              SpatialDataSource::SourceType type,
                              const LLBBox& region) {
        // Boost OSMSHP priority in regions with known good coverage
        if (type == SpatialDataSource::SourceType::OSMSHP) {
            if (IsWellMappedRegion(region)) {
                return basePriority + 10;
            }
        }
        return basePriority;
    }
};
```

#### 3. Memory Management Integration

**Leverage Existing Caching**: Build upon the existing chart caching infrastructure discovered in Task 1.1:

```cpp
class UnifiedSpatialCache {
private:
    // Integrate with existing OpenCPN cache patterns
    std::unordered_map<std::string, std::weak_ptr<SpatialFeature>> m_featureCache;
    std::chrono::time_point<std::chrono::steady_clock> m_lastCleanup;
    
public:
    // Respect existing memory pressure handling
    void OnMemoryPressure() {
        // Use same patterns as existing chart cache
        CleanupExpiredFeatures();
        ReduceCacheSize();
    }
    
    // Coordinate with existing cache systems
    void IntegrateWithChartCache(ChartBase* chart) {
        // Share memory budget with chart rendering cache
        // Avoid duplicate coastline data between chart and spatial cache
    }
};
```

#### 4. Coordinate System Integration

**Unified Coordinate Handling**: Address the coordinate system inconsistencies found in Task 1.1:

```cpp
class CoordinateManager {
public:
    // Normalize coordinates from different data sources
    static wxRealPoint NormalizeCoordinate(double lat, double lon, 
                                          SpatialDataSource::SourceType source) {
        switch (source) {
            case SpatialDataSource::SourceType::GSHHS:
                // GSHHS uses 0-360° longitude
                if (lon > 180.0) lon -= 360.0;
                break;
            case SpatialDataSource::SourceType::S57_ENC:
                // S-57 may have different datum - transform to WGS84
                // Use existing OpenCPN coordinate transformation utilities
                break;
            case SpatialDataSource::SourceType::OSMSHP:
                // Already in standard -180/+180° format
                break;
        }
        return wxRealPoint(lon, lat);
    }
    
    // Handle Greenwich meridian crossing consistently
    static LLBBox NormalizeBounds(const LLBBox& bounds) {
        // Use existing OpenCPN bounding box utilities
        return bounds.GetCanonicalBox();
    }
};
```

#### 5. Performance Optimization Strategy

**Address Current Bottlenecks**: Based on the O(n) performance issues identified in Task 1.1:

```cpp
class PerformanceOptimizedQuery {
public:
    // Replace linear search with spatial indexing
    static bool FastCrossesLand(const wxLineF& trajectory,
                               const std::vector<std::unique_ptr<SpatialFeature>>& features) {
        // Pre-filter using bounding box intersection (O(log n))
        auto candidates = m_spatialIndex.Query(trajectory.GetBoundingBox());
        
        // Only test actual intersection for spatially nearby features
        for (const auto& feature : candidates) {
            if (feature->IntersectsLine(trajectory)) {
                return true;
            }
        }
        return false;
    }
    
private:
    static SpatialIndex m_spatialIndex; // R-tree or similar
};
```

#### 6. Plugin Integration Framework

**Extensible Plugin Support**: Enable plugins to contribute spatial data as identified in Task 1.1:

```cpp
// Plugin registration interface
class PluginSpatialInterface {
public:
    virtual ~PluginSpatialInterface() = default;
    
    // Allow plugins to provide custom spatial data
    virtual std::vector<std::unique_ptr<SpatialFeature>> 
        GetSpatialFeatures(const LLBBox& bounds) = 0;
    
    // Plugin metadata
    virtual std::string GetPluginName() const = 0;
    virtual int GetDataQuality() const = 0;
    virtual LLBBox GetDataCoverage() const = 0;
};

// Registration mechanism
extern "C" void RegisterPluginSpatialData(PluginSpatialInterface* interface) {
    g_DataSourceManager->RegisterPluginSource(
        std::make_unique<PluginDataSourceAdapter>(interface));
}
```

### Technical Decisions and Rationale

#### 1. Why Hierarchical Feature Types?

The analysis revealed that different data sources have very different feature classification schemes:
- **GSHHS**: 5 hierarchical levels (land/lake/island/pond)
- **S-57**: 100+ object classes with complex attributes  
- **OSMSHP**: Simple land/water binary classification

The unified model preserves this richness while providing common interfaces.

#### 2. Why Priority-Based Data Source Selection?

Task 1.1 revealed that users have different combinations of data sources, and forcing cross-referencing would be impractical. Priority-based selection automatically uses the best available data without requiring all sources to be present.

#### 3. Why Adapter Pattern for Data Sources?

The diverse data formats (binary GSHHS, GDAL S-57, Shapefile) require different loading mechanisms. The adapter pattern allows the system to be extended with new formats without modifying core logic.

#### 4. Why Streaming Feature Extraction?

Large ENC charts or high-resolution coastline data can contain millions of features. Streaming extraction prevents memory exhaustion and enables progressive loading based on viewport requirements.

### Next Steps for Task 1.3

This abstraction layer provides the foundation for the high-performance spatial indexing system in Task 1.3:

1. **Spatial Index Integration**: The unified features will be indexed using R-tree or similar spatial acceleration structure
2. **Query Optimization**: The common interface enables optimization of spatial queries across all data sources
3. **Pathfinding API**: The feature abstractions provide the building blocks for safe route calculation
4. **Performance Validation**: The adapter framework enables A/B testing between old and new systems
