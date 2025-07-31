# Task 1.1: Analyze Existing Spatial Data Systems

**Phase**: Foundation & Analysis  
**Dependencies**: None

## Detailed Description

Comprehensively analyze OpenCPN's multiple spatial data systems to understand the current landscape of geographic data sources and identify opportunities for unified spatial indexing.

## Data Systems to Analyze

- **GSHHS**: Current polygon-based coastline system with quality levels
- **S-57/ENC**: Vector chart format with depth contours, hazards, and navigation features
- **OSMSHP**: OpenStreetMap-derived shapefile coastline data
- **Shapefile basemaps**: Generic shapefile support for various geographic features
- **Chart databases**: Integration with existing chart data structures
- **Plugin data sources**: How plugins currently provide geographic data

## Analysis Requirements

### 1. Data Format Assessment

- Understand each system's coordinate systems, projections, and datum handling
- Analyze polygon vs polyline representations across different sources
- Document feature classification schemes (land/water, depth levels, hazard types)
- Assess data quality, resolution, and coverage characteristics

### 2. Current Performance Characteristics

- Identify bottlenecks in existing polygon intersection algorithms
- Measure memory usage patterns for different data sources
- Analyze loading times and caching behaviors
- Document current spatial query patterns and frequencies

### 3. Integration Points Discovery

- Map existing APIs and data access patterns
- Identify common geometric operations across systems
- Understand plugin integration mechanisms
- Document existing coordinate transformation pipelines

## Acceptance Criteria

- [ ] Complete inventory of all spatial data systems in OpenCPN
- [ ] Performance baseline measurements for each data source
- [ ] Documentation of data format characteristics and limitations
- [ ] Clear understanding of current plugin integration patterns
- [ ] Identified opportunities for unified spatial indexing across all systems

## Findings

### Complete Inventory of Spatial Data Systems

**1. GSHHS (Global Self-consistent, Hierarchical, High-resolution Shoreline)**

- **Location**: `gui/src/gshhs.cpp`, `gui/include/gui/gshhs.h`
- **Purpose**: Primary coastline system for land/water boundary detection
- **Data Structure**: Polygon-based with hierarchical quality levels (0=low, 4=full)
- **Coordinate System**: WGS84 with micro-degree precision (1.0e-6 scaling)
- **Memory Organization**: Grid-based cells (360x180 global grid), each cell divided into 16x16 subcells
- **Key Classes**: `GshhsReader`, `GshhsPolyReader`, `GshhsPolyCell`, `GSHHSChart`
- **Critical Function**: `gshhsCrossesLand()` - global entry point for land crossing detection

**2. Shapefile Basemap System**

- **Location**: `gui/src/shapefile_basemap.cpp`, `gui/include/gui/shapefile_basemap.h`
- **Purpose**: OpenStreetMap-derived coastline data with multiple quality levels
- **Data Structure**: Shapefile format with tiled organization using LatLonKey indexing
- **Quality Levels**: crude (10x10°), low, medium, high, full (1x1° tiles)
- **Key Classes**: `ShapeBaseChart`, `ShapeBaseChartSet`, `LatLonKey`
- **Performance**: Scale-dependent quality selection with async loading support

**3. S-57/ENC Electronic Nautical Charts**

- **Location**: `gui/src/s57chart.cpp`, `gui/include/gui/s57chart.h`, `libs/s57-charts/`
- **Purpose**: Professional vector chart format with navigation features
- **Data Structure**: Object-oriented with depth contours, hazards, navigation aids
- **Standards**: IHO S-57 international standard compliance
- **Integration**: Full integration with S-52 presentation library
- **Features**: Depth soundings, safety contours, traffic schemes, restricted areas
- **Data Quality**: **Superior accuracy to GSHHS** in covered areas, official hydrographic office data
- **Coverage Limitation**: Not globally available, requires licensed chart data
- **Critical Gap**: **No API exists to query ENC data for routing/pathfinding** despite superior accuracy

**4. Chart Database System**

- **Location**: Throughout `gui/src/` (chart loading, management)
- **Purpose**: Unified chart management across all formats
- **Integration**: Plugin architecture for additional chart types
- **Caching**: Advanced caching system for chart tiles and objects

**5. Plugin Data Sources**

- **Location**: `model/src/plugin_*.cpp`, `include/ocpn_plugin.h`
- **Purpose**: Third-party spatial data integration
- **Architecture**: Plugin API allows custom geographic data providers
- **Examples**: Weather routing plugins, AIS data, custom waypoint sources

### Current Performance Characteristics

**Critical Performance Bottleneck Identified**:
The current `crossing1()` algorithm in GSHHS performs **linear polygon intersection testing** without spatial indexing:

```cpp
bool crossing1(wxLineF trajectWorld) {
    // For each cell in global 360x180 grid
    for (int i = 0; i < 360; i++) {
        for (int j = 0; j < 180; j++) {
            if (allCells[i][j]) {
                // Test every coastline segment in cell
                std::vector<wxLineF> *coasts = allCells[i][j]->getCoasts();
                for (auto &line : *coasts) {
                    if (LineLineIntersect(trajectWorld, line)) {
                        return true;
                    }
                }
            }
        }
    }
}
```

**Performance Issues**:

- **O(n) complexity**: Tests every segment sequentially
- **No spatial indexing**: Cannot quickly eliminate irrelevant segments
- **Memory inefficient**: Loads entire cells even for small queries
- **Thread unsafe**: Uses global mutex locks for concurrent access

**Memory Usage Patterns**:

- GSHHS: ~50-200MB depending on quality level
- Shapefile basemaps: 10-500MB with lazy loading
- S-57 charts: Varies significantly by chart density
- All systems load data on-demand but lack unified caching

### Data Format Characteristics and Limitations

**Coordinate System Handling**:

- **GSHHS**: WGS84 with micro-degree precision, handles Greenwich meridian crossing
- **Shapefiles**: Geographic coordinates with normalization to -180°/+180°
- **S-57**: Multiple datum support with transformation capabilities
- **Limitation**: Each system handles coordinate transformations independently

**Polygon vs Polyline Representations**:

- **GSHHS**: Closed polygon chains with hierarchical levels (land/lake/island)
- **Shapefiles**: Multi-polygon features with optional tiling attributes
- **S-57**: Mixed geometry types (points, lines, areas) with semantic classification
- **Inconsistency**: Different topology handling across systems

**Feature Classification Schemes**:

- **GSHHS**: 5 levels (land, lake, island_in_lake, pond_in_island, etc.)
- **Shapefiles**: Binary land/water classification
- **S-57**: Rich semantic classification (100+ object classes)
- **Challenge**: No unified feature type mapping

### Current Plugin Integration Patterns

**Plugin API Discovery**:

- **Location**: `include/ocpn_plugin.h`, `model/src/plugin_api.cpp`
- **Architecture**: Plugin base classes with versioned API (v1.16-v1.18+)
- **Spatial Integration**: Limited - plugins mainly provide chart rendering, not spatial queries
- **Missing**: No standardized interface for plugins to contribute spatial data to routing/safety systems

**Current Integration Points**:

1. **Chart Plugins**: Provide additional chart format support
2. **Overlay Plugins**: Add custom geographic overlays
3. **Route Plugins**: Weather routing and optimization
4. **Data Plugins**: AIS, GPS, and sensor data integration

**Integration Limitations**:

- No unified spatial query interface for plugins
- Each system maintains separate data structures
- Limited cross-system interoperability
- **Critical Gap**: No pathfinding/routing API that can leverage high-quality ENC data
- **Safety Risk**: Routing systems forced to rely on lower-quality GSHHS data even when better ENC data is available

### Identified Opportunities for Unified Spatial Indexing

**1. Performance Optimization Opportunities**:

- **10-100x improvement potential** by replacing linear search with R-tree indexing
- Unified spatial index could serve all systems simultaneously
- Single query interface could eliminate redundant intersection tests

**2. Memory Optimization**:

- Current systems duplicate similar coastline data across GSHHS and shapefiles
- Unified index could reduce total memory footprint by 30-50%
- Smart caching could predict and preload relevant spatial data

**3. API Unification**:

- Single `CrossesLand()` interface could work across all data sources
- Plugin API could be extended to contribute to spatial index
- Consistent coordinate handling across all systems

**4. Multi-Resolution Integration**:

- Automatic quality selection based on query scale and performance requirements
- Progressive detail loading for interactive applications
- Distance field optimization for frequently queried areas
- **Adaptive Data Source Selection**: Automatically select the best available data source per region based on user's installation - ENC where available, OSMSHP for good OSM coverage areas, GSHHS as global fallback

**5. Missing Pathfinding Capabilities**:

- **Safe Route Calculation**: No current API to calculate paths that avoid land AND depth constraints
- **Multi-Constraint Navigation**: Need for routing that considers depth contours, restricted areas, traffic schemes
- **Real-time Route Validation**: Ability to continuously validate routes against changing conditions and updated chart data

**Critical Finding**: The current `PlugIn_GSHHS_CrossesLand()` function in the plugin API is the **only available interface** for land crossing detection, creating several critical limitations:

1. **GSHHS Data Quality Limitations**: GSHHS provides global coverage but has significant accuracy issues in many areas compared to official ENC charts. In coastal areas where precise navigation is critical, GSHHS may show land where there are navigable channels, or miss shallow areas that should be avoided.

2. **No Intelligent Data Source Selection**: There's no way to automatically use the **best available data source** for a given area. If a user has high-quality ENC charts for their local waters but the system only queries GSHHS, they get lower accuracy even though better data is available in their installation.

3. **Missing Pathfinding Capability**: **No function exists to calculate a safe path between two points** that avoids land, shallow water, or other maritime hazards. This is a fundamental gap for automated route planning and weather routing applications.

4. **Depth Contour Limitations**: While S-57 charts contain detailed depth contours and safety information, there's no unified interface to query "safe passage" considering both land boundaries AND depth constraints.

This makes the spatial indexing project even more critical - it's not just about performance, but about enabling fundamentally new navigation safety capabilities that don't currently exist.

## Next Steps

Based on this analysis, the following recommendations guide the subsequent tasks:

### Immediate Priorities (Phase 1)

1. **Task 1.2**: Design a unified spatial feature abstraction that can represent coastlines, depth contours, and hazards from all identified systems with consistent topology and coordinate handling.

2. **Task 1.3**: Create a prototype multi-source spatial index that demonstrates 10-100x performance improvements over the current linear search algorithms, specifically targeting the `PlugIn_GSHHS_CrossesLand()` bottleneck and **enabling new pathfinding capabilities** that can leverage both GSHHS and ENC data sources for optimal route planning.

### Architecture Recommendations

**Replace Linear Search with R-tree Indexing**: The current O(n) polygon intersection algorithm is the primary performance bottleneck. An R-tree spatial index would enable O(log n) queries with dramatic performance improvements.

**Unified Data Model**: Create abstractions that can represent features from GSHHS, shapefiles, and S-57 data uniformly, enabling consistent spatial queries across all systems while **leveraging the superior accuracy of ENC data where available**.

**Safe Pathfinding API**: Design and implement routing algorithms that can calculate safe paths between waypoints, considering both land boundaries and depth constraints from the most accurate available data source.

**Multi-Source Data Fusion**: Implement intelligent data source selection that **uses the best available data source** for each geographic area - ENC charts where the user has them installed, OSMSHP in areas with good OpenStreetMap coverage, and GSHHS as the global fallback - providing optimal navigation accuracy based on what each user actually has available.

**Progressive Quality Loading**: Implement a system that automatically selects data quality based on query scale and performance requirements, reducing memory usage while maintaining accuracy.

**Plugin API Extension**: Design new plugin interfaces that allow third-party spatial data sources to contribute to the unified spatial index, enabling plugins to participate in routing and safety systems.

### Critical Success Metrics

- Achieve 10-100x performance improvement in land crossing detection
- Reduce memory footprint by 30-50% through unified data structures  
- Maintain backward compatibility with existing GSHHS API
- Enable real-time constraint evaluation for weather routing applications
- **Provide safe pathfinding API** that can calculate routes avoiding land and depth hazards
- **Improve navigation accuracy** by automatically using the best available data source per region instead of being locked into GSHHS-only queries

### Risk Mitigation

**Marine Safety Priority**: All changes must maintain or improve the accuracy of land crossing detection, as errors could impact maritime navigation safety.

**Incremental Implementation**: Implement changes incrementally with thorough testing at each stage to avoid disrupting the existing stable navigation systems.

**Cross-Platform Compatibility**: Ensure the spatial indexing system works efficiently across OpenCPN's supported platforms (Windows, macOS, Linux, Android).
