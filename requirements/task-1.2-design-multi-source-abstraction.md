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

Based on Task 1.1 analysis, the multi-source spatial abstraction must handle four well-understood data systems:

1. **GSHHS** - Global polygon coastlines (5 quality levels: crude=1:50M to full=1:200K)
2. **OSMSHP** - OpenStreetMap shapefile coastlines (5 quality levels: 10x10° to 1x1° tiles)
3. **S-57/ENC** - Professional vector charts (variable resolution, typically 1:500 to 1:50,000)
4. **Shapefile Basemaps** - Generic shapefile features (variable resolution)

**Key Design Challenge**: These sources have vastly different resolutions - from GSHHS crude (0.1° vertex spacing) to ENC charts (sub-meter accuracy). The abstraction must handle this resolution diversity intelligently.

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
    virtual double GetResolution() const = 0;  // Average spacing between vertices in degrees
    
    // Feature-specific attributes
    virtual std::map<std::string, std::any> GetAttributes() const = 0;
};

// Specialized feature types
class CoastlineFeature : public SpatialFeature {
private:
    std::vector<wxRealPoint> m_vertices;
    int m_gshhsLevel; // 0=land, 1=lake, 2=island_in_lake, etc.
    bool m_isLand;    // true=land, false=water
    double m_resolution; // Average distance between vertices in degrees
    std::string m_dataSource; // "GSHHS", "OSMSHP", "ENC", etc.
    
public:
    FeatureType GetFeatureType() const override { return FeatureType::COASTLINE; }
    bool IsLandBoundary() const { return m_isLand; }
    int GetGshhsHierarchy() const { return m_gshhsLevel; }
    const std::vector<wxRealPoint>& GetVertices() const { return m_vertices; }
    double GetResolution() const override { return m_resolution; }
    std::string GetDataSource() const override { return m_dataSource; }
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
        SHAPEFILE
        // PLUGIN support to be added after further analysis
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
    
    // Get all available data sources for a region, ordered by resolution
    std::vector<SpatialDataSource*> GetAvailableSources(
        const LLBBox& region) const;
    
    // Let user/application decide which source to use based on specific needs
    std::vector<SpatialDataSource*> GetSourcesByType(
        SpatialDataSource::SourceType type) const;
    
    // Register plugin data sources at runtime (future enhancement)
    // void RegisterPluginSource(std::unique_ptr<SpatialDataSource> source);
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

### Data Source Selection Strategy

The key insight is that **pathfinding should use the highest resolution data available**, regardless of display scale. However, we cannot make assumptions about which data source is "better" between OSMSHP (crowdsourced) and ENC (official) since they have different characteristics:

```cpp
class DataSourceStrategy {
public:
    // Simple approach: Let the application choose the strategy
    enum class SelectionMode {
        HIGHEST_RESOLUTION,    // Use finest resolution available
        SPECIFIC_SOURCE,       // Use only a particular source type
        ALL_AVAILABLE         // Return all sources, let caller decide
    };
    
    static std::vector<SpatialDataSource*> SelectSources(
        const DataSourceManager& manager,
        const LLBBox& region,
        SelectionMode mode,
        SpatialDataSource::SourceType preferredType = SpatialDataSource::SourceType::GSHHS) {
        
        switch (mode) {
            case SelectionMode::HIGHEST_RESOLUTION:
                return SelectByResolution(manager, region);
            case SelectionMode::SPECIFIC_SOURCE:
                return manager.GetSourcesByType(preferredType);
            case SelectionMode::ALL_AVAILABLE:
                return manager.GetAvailableSources(region);
        }
        return {};
    }
    
private:
    static std::vector<SpatialDataSource*> SelectByResolution(
        const DataSourceManager& manager, const LLBBox& region) {
        auto sources = manager.GetAvailableSources(region);
        
        // Sort by resolution (finest first)
        std::sort(sources.begin(), sources.end(), 
            [](const SpatialDataSource* a, const SpatialDataSource* b) {
                // This is a simplification - actual resolution comparison 
                // would need to consider the source type and quality level
                return a->GetQualityLevel() > b->GetQualityLevel();
            });
        
        return sources;
    }
};
```

**Key Design Principles**:

1. **No Assumptions About Data Quality**: Don't try to rank OSMSHP vs ENC - they serve different purposes
2. **Application-Driven Selection**: Let the pathfinding algorithm specify what it needs
3. **Resolution-Based When Possible**: Within a data source type, higher resolution is generally better
4. **Fallback Capability**: Always provide GSHHS as global fallback

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

#### 2. Data Source Selection Logic

Based on Task 1.1 findings, implement flexible data source selection without assumptions about relative quality:

```cpp
class DataSourceSelection {
public:
    // Don't make assumptions about which source is "better" - let application decide
    static std::vector<SpatialDataSource*> GetSourcesForPathfinding(
        const DataSourceManager& manager, const LLBBox& region) {
        
        // For pathfinding: get ALL available sources, ordered by resolution
        auto sources = manager.GetAvailableSources(region);
        
        // Sort by resolution within each source type
        std::sort(sources.begin(), sources.end(), 
            [](const SpatialDataSource* a, const SpatialDataSource* b) {
                // First group by source type, then by quality level within type
                if (a->GetSourceType() != b->GetSourceType()) {
                    return static_cast<int>(a->GetSourceType()) < static_cast<int>(b->GetSourceType());
                }
                return a->GetQualityLevel() > b->GetQualityLevel();
            });
        
        return sources;
    }
    
    // For display: might want different logic (but that's not our focus)
    static SpatialDataSource* GetSourceForDisplay(
        const DataSourceManager& manager, const LLBBox& region, double scale) {
        // This would be display-specific logic (future enhancement)
        return nullptr;
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

#### 6. Future Plugin Integration

**Note**: Plugin spatial data integration requires further analysis of the plugin architecture and is deferred to future tasks. Current focus is on the four well-understood data sources: GSHHS, OSMSHP, S-57/ENC, and generic Shapefiles.

### Technical Decisions and Rationale

#### 1. Why No Assumptions About Data Source Quality?

You're absolutely right that we can't easily compare OSMSHP (crowdsourced) vs ENC (official) data quality:

- **OSMSHP**: Quality depends on local crowdsourcing - excellent in some areas, poor in others
- **ENC**: Official hydrographic office data but may not be available or up-to-date everywhere  
- **GSHHS**: Global coverage but lower resolution and accuracy

Rather than guess which is "better", the abstraction provides all available sources and lets the application decide based on its specific needs.

#### 2. Why Resolution-Independent Pathfinding?

For navigation safety, pathfinding should use the highest resolution data available, completely independent of display scale:

- A world-view chart display still needs precise pathfinding for safety
- Display scale and computational accuracy are separate concerns
- Always err on the side of using more detailed data for route calculation

#### 3. Why Application-Driven Selection?

Different use cases need different data source strategies:

- **Pathfinding**: Wants highest resolution available
- **Chart Display**: Might want scale-appropriate data for performance
- **Route Validation**: Might want to check against multiple sources

The abstraction provides the tools but doesn't impose policy.

### Next Steps for Task 1.3

This abstraction layer provides the foundation for the high-performance spatial indexing system in Task 1.3:

1. **Spatial Index Integration**: The unified features will be indexed using R-tree or similar spatial acceleration structure
2. **Query Optimization**: The common interface enables optimization of spatial queries across all data sources
3. **Pathfinding API**: The feature abstractions provide the building blocks for safe route calculation
4. **Performance Validation**: The adapter framework enables A/B testing between old and new systems
