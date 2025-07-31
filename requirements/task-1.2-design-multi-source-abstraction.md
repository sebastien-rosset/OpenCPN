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

### OpenCPN's Existing Coastline Architecture

**Critical Discovery**: OpenCPN already has significant coastline processing infrastructure, but each chart type uses separate pipelines with performance bottlenecks for spatial queries.

#### Existing Common Data Structures

All chart types converge on shared polygon representations for rendering:

```cpp
// SHARED geometric representation across all chart types:
typedef std::vector<wxRealPoint> contour;           // Single polygon boundary  
typedef std::vector<contour> contour_list;          // Multiple polygons with holes
```

Used by:

- **GSHHS**: `contour_list poly1, poly2, poly3, poly4, poly5` (land/water hierarchy levels)
- **OSMSHP/Shapefiles**: Same `contour` and `contour_list` internal representation
- **S-57/ENC**: OGR geometries converted to similar structures via `OGRPolygon`

#### Existing Land Detection Functions

**GSHHS System**:

- `PlugIn_GSHHS_CrossesLand(lat1, lon1, lat2, lon2)` - Global plugin API (GSHHS only)
- `getCoasts()` method returns `std::vector<wxLineF>` line segments
- Quality level functions: `PlugInGetMinAvailableGshhgQuality()`, `PlugInGetMaxAvailableGshhgQuality()`

**OSMSHP/Shapefile System**:

- `ShapeBaseChart::CrossesLand(lat1, lon1, lat2, lon2)` - **Exists but not exposed to plugin API!**
- Tiled organization with 1×1° or 10×10° cells
- **Performance Issue**: Functionally working but unacceptable performance for OSMSHP files

**S-57/ENC System**:

- Complex OGR geometry processing for **LNDARE** (land areas) and **COALNE** (coastline) objects
- No unified `CrossesLand()` API currently exposed
- **Performance Unknown**: ENC chart `CrossesLand()` performance not yet tested

#### Key Performance Challenge

**The core problem**: While land detection functions exist, they use **O(n) linear polygon intersection algorithms** that become prohibitively slow with high-resolution data:

- **GSHHS**: Acceptable performance due to lower resolution
- **OSMSHP**: **Unacceptable performance** despite functional correctness  
- **S-57/ENC**: Performance characteristics unknown but likely similar issues

### Core Architecture Overview

Based on analysis of existing OpenCPN systems, the multi-source spatial abstraction must handle four data systems with a **unified high-performance indexing layer**:

1. **GSHHS** - Global polygon coastlines (5 quality levels: crude=1:50M to full=1:200K)
2. **OSMSHP** - OpenStreetMap shapefile coastlines (5 quality levels: 10x10° to 1x1° tiles)
3. **S-57/ENC** - Vector charts (variable resolution, typically 1:500 to 1:50,000)
4. **Shapefile Basemaps** - Generic shapefile features (variable resolution)

**Key Design Challenge**: Replace O(n) linear intersection algorithms with **efficient spatial indexing (R-tree)** to achieve acceptable performance across all resolution levels, from GSHHS crude (0.1° vertex spacing) to ENC charts (sub-meter accuracy).

### Revised Unified Feature Model - Zero-Copy Interface Design

**Key Insight**: Rather than copying data into common structures, design interfaces that existing OpenCPN data structures can implement directly. This eliminates data duplication and leverages existing optimized memory layouts.

**Core Principle**: **Zero-Copy Abstraction** - Spatial indexing operates over existing data structures without duplication.

```cpp
// Interface that existing data structures can implement directly
// 
// TWO-STAGE SPATIAL QUERY ARCHITECTURE:
// 1. R-tree spatial index provides fast pre-filtering using GetSegmentBounds()
// 2. IntersectsLine() performs precise intersection testing on surviving candidates
//
// PERFORMANCE FLOW:
// FastCrossesLand() -> R-tree.Query() -> candidates -> IntersectsLine() for each candidate
//
// This eliminates O(n) linear search while preserving existing intersection algorithms
//
// CRITICAL PERFORMANCE CONTEXT:
// Maritime polygon data can contain TENS OF MILLIONS of vertices in single polygons
// (e.g., high-resolution Pacific coastline, detailed island chains)
// Traditional linear intersection algorithms become prohibitively slow at this scale
class ICoastlineGeometry {
public:
    virtual ~ICoastlineGeometry() = default;
    
    // STAGE 2: Precise intersection testing - called AFTER R-tree pre-filtering
    // This delegates to existing CrossesLand()/crossing1() implementations
    // Only called on geometries that survived spatial pre-filtering
    // 
    // PERFORMANCE WARNING: Some polygons contain tens of millions of vertices
    // Even with R-tree pre-filtering, this may still be expensive for coastal paths
    virtual bool IntersectsLine(double lat1, double lon1, double lat2, double lon2) const = 0;
    
    // DATASET BOUNDS: Returns bounding box for the ENTIRE geometry dataset
    // For GSHHS: bounds of all polygons in the cell
    // For OSMSHP: bounds of all polygons in the shapefile/chart
    // For S-57: bounds of all LNDARE objects in the chart
    // Used for: data source selection, coverage analysis, R-tree root bounds
    virtual LLBBox GetBoundingBox() const = 0;
    
    virtual std::string GetDataSourceName() const = 0;
    virtual int GetQualityLevel() const = 0;
    
    // STAGE 1: Spatial indexing preparation - UNIFIED SUBDIVISION INTERFACE
    // 
    // SUBDIVISION REQUIREMENTS for high performance:
    // 1. Large polygons (>1° span) MUST be subdivided into smaller segments
    // 2. Segments should target ~1° maximum dimension (roughly 111km at equator)
    // 3. Segments MUST overlap by 5-10 vertices to prevent intersection gaps
    // 4. Small polygons (<1° or <10 vertices) should remain as single segments
    // 5. Subdivision is for INDEX ACCELERATION only - original geometry unchanged
    //
    // IMPLEMENTATION: Delegates to unified subdivision algorithm, not per-adapter logic
    virtual std::vector<LLBBox> GetSegmentBounds() const = 0;
    
    // GEOMETRY ACCESS INTERFACE: Support for unified subdivision algorithm
    // Provides structured access to underlying polygon data for subdivision
    virtual size_t GetContourCount() const = 0;
    virtual size_t GetContourVertexCount(size_t contourIndex) const = 0;
    virtual wxRealPoint GetContourVertex(size_t contourIndex, size_t vertexIndex) const = 0;
    
    // NATIVE GEOMETRY ACCESS: Direct access to original data structures
    // 
    // PRIMARY USES:
    // 1. **Performance-Critical Code**: Direct access avoids interface overhead
    //    - Custom optimized intersection algorithms
    //    - Bulk geometry processing operations
    //    - Real-time rendering optimizations
    // 
    // 2. **Memory Analysis & Debugging**: 
    //    - Measure actual memory usage of original data structures
    //    - Validate zero-copy design (no data duplication)
    //    - Debug geometry corruption or coordinate system issues
    // 
    // 3. **Legacy Integration**: 
    //    - Existing code that expects specific data structure types
    //    - Gradual migration from direct data access to interface-based access
    //    - Plugin compatibility where direct structure access is required
    // 
    // 4. **Advanced Spatial Operations**:
    //    - Complex geometric algorithms that need full polygon access
    //    - Coordinate transformations that operate on entire datasets
    //    - Statistical analysis of vertex distributions
    // 
    // USAGE PATTERN:
    // ```cpp
    // if (geometry->GetNativeType() == "contour_list") {
    //     const contour_list* polygons = static_cast<const contour_list*>(
    //         geometry->GetNativeGeometry());
    //     // Direct access to wxRealPoint vertices for performance-critical operations
    // }
    // ```
    virtual const void* GetNativeGeometry() const = 0;
    virtual std::string GetNativeType() const = 0;  // "contour_list", "OGRPolygon", etc.
};

// UNIFIED SUBDIVISION ALGORITHM: Single implementation used by all adapters
// Eliminates code duplication and provides consistent subdivision behavior
class UnifiedPolygonSubdivision {
public:
    struct SubdivisionParams {
        double maxDegreeSpan = 1.0;           // Maximum segment span in degrees
        size_t maxVerticesPerSegment = 50;    // Target vertices per segment
        size_t overlapVertices = 5;           // Overlap between adjacent segments
        size_t minVerticesForSubdivision = 10; // Don't subdivide tiny polygons
    };
    
    // SINGLE SUBDIVISION ALGORITHM: Works with any ICoastlineGeometry implementation
    static std::vector<LLBBox> SubdivideGeometry(const ICoastlineGeometry& geometry, 
                                                 const SubdivisionParams& params = SubdivisionParams{}) {
        std::vector<LLBBox> allSegmentBounds;
        
        // Process each contour in the geometry
        for (size_t contourIdx = 0; contourIdx < geometry.GetContourCount(); ++contourIdx) {
            auto contourBounds = SubdivideContour(geometry, contourIdx, params);
            allSegmentBounds.insert(allSegmentBounds.end(), contourBounds.begin(), contourBounds.end());
        }
        
        return allSegmentBounds;
    }
    
private:
    // Subdivide individual contour using sliding window approach
    static std::vector<LLBBox> SubdivideContour(const ICoastlineGeometry& geometry, 
                                               size_t contourIndex,
                                               const SubdivisionParams& params) {
        std::vector<LLBBox> segmentBounds;
        
        size_t vertexCount = geometry.GetContourVertexCount(contourIndex);
        
        // Calculate contour bounding box
        LLBBox contourBounds;
        for (size_t i = 0; i < vertexCount; ++i) {
            wxRealPoint vertex = geometry.GetContourVertex(contourIndex, i);
            contourBounds.Expand(vertex.y, vertex.x);  // lat, lon
        }
        
        // Check if subdivision is needed
        double maxDimension = std::max(contourBounds.GetLonRange(), contourBounds.GetLatRange());
        if (maxDimension <= params.maxDegreeSpan || vertexCount < params.minVerticesForSubdivision) {
            // Small contour - use as single segment
            segmentBounds.push_back(contourBounds);
            return segmentBounds;
        }
        
        // SLIDING WINDOW SUBDIVISION: Create overlapping segments
        for (size_t start = 0; start < vertexCount; start += (params.maxVerticesPerSegment - params.overlapVertices)) {
            size_t end = std::min(start + params.maxVerticesPerSegment, vertexCount);
            
            // Calculate bounding box for this vertex range
            LLBBox segmentBox;
            for (size_t i = start; i < end; ++i) {
                wxRealPoint vertex = geometry.GetContourVertex(contourIndex, i);
                segmentBox.Expand(vertex.y, vertex.x);
            }
            
            segmentBounds.push_back(segmentBox);
            
            if (end >= vertexCount) break;
        }
        
        return segmentBounds;
    }
};

// Extend existing GSHHS structures to implement the interface
class GshhsPolyCellAdapter : public ICoastlineGeometry {
private:
    GshhsPolyCell* m_originalCell;  // Reference to existing data - NO COPY
    
public:
    explicit GshhsPolyCellAdapter(GshhsPolyCell* cell) : m_originalCell(cell) {}
    
    bool IntersectsLine(double lat1, double lon1, double lat2, double lon2) const override {
        // STAGE 2: Precise intersection testing using existing GSHHS algorithm
        // This is called ONLY on candidates that survived R-tree spatial pre-filtering
        // 
        // PERFORMANCE CONTEXT:
        // - Before: O(n) - this was called on ALL GSHHS cells
        // - After: O(log n + k) - R-tree reduces candidates to small set, then we test each
        // 
        // PERFORMANCE WARNING: GSHHS polygons can contain millions of vertices
        // Coastal navigation paths may still trigger many calls to this method
        // 
        // No data duplication - delegates to existing proven intersection code
        return m_originalCell->crossing1(/* trajectory parameters */);
    }
    
    LLBBox GetBoundingBox() const override {
        // DATASET BOUNDS: Return bounds for ALL polygons in this GSHHS cell
        // Covers poly1, poly2, poly3, poly4, poly5 (different land/water hierarchy levels)
        return m_originalCell->GetBoundingBox();
    }
    
    std::vector<LLBBox> GetSegmentBounds() const override {
        // UNIFIED SUBDIVISION: Delegate to single algorithm instead of per-adapter logic
        return UnifiedPolygonSubdivision::SubdivideGeometry(*this);
    }
    
    // GEOMETRY ACCESS INTERFACE: Support unified subdivision algorithm
    size_t GetContourCount() const override {
        // GSHHS has multiple polygon levels: poly1, poly2, poly3, poly4, poly5
        const contour_list& polygons = m_originalCell->getPoly1();
        return polygons.size();
    }
    
    size_t GetContourVertexCount(size_t contourIndex) const override {
        const contour_list& polygons = m_originalCell->getPoly1();
        if (contourIndex >= polygons.size()) return 0;
        return polygons[contourIndex].size();
    }
    
    wxRealPoint GetContourVertex(size_t contourIndex, size_t vertexIndex) const override {
        const contour_list& polygons = m_originalCell->getPoly1(); 
        if (contourIndex >= polygons.size()) return wxRealPoint(0, 0);
        if (vertexIndex >= polygons[contourIndex].size()) return wxRealPoint(0, 0);
        
        const wxRealPoint& vertex = polygons[contourIndex][vertexIndex];
        return vertex; // Already in lat/lon format
    }
    
    const void* GetNativeGeometry() const override {
        return &m_originalCell->getPoly1();  // Direct access to existing contour_list
    }
    
    std::string GetNativeType() const override { return "contour_list"; }
    std::string GetDataSourceName() const override { return "GSHHS"; }
};

// OSMSHP/OpenStreetMap Shapefile Adapter
// 
// DATASET: OpenStreetMap-derived coastline data in shapefile format
// SOURCE: Processed OSM coastline data, available in multiple quality levels
// QUALITY LEVELS: 5 levels from 10×10° tiles (crude) to 1×1° tiles (full detail)
// PERFORMANCE ISSUE: **PRIMARY TARGET FOR OPTIMIZATION**
// - ShapeBaseChart::CrossesLand() exists but has UNACCEPTABLE performance
// - Contains some of the largest polygons in maritime data (entire ocean coastlines)
// - Polygons can contain TENS OF MILLIONS of vertices
// - Critical for high-resolution coastal navigation but currently unusable due to speed
class ShapeBaseChartAdapter : public ICoastlineGeometry {
private:
    ShapeBaseChart* m_originalChart;  // Reference to existing data - NO COPY
    
public:
    explicit ShapeBaseChartAdapter(ShapeBaseChart* chart) : m_originalChart(chart) {}
    
    bool IntersectsLine(double lat1, double lon1, double lat2, double lon2) const override {
        // STAGE 2: Precise intersection using existing OSMSHP/shapefile algorithm
        // 
        // CRITICAL PERFORMANCE CONCERN: OSMSHP polygons are MASSIVE
        // - Single polygons can contain 10+ million vertices (entire Pacific coastline)
        // - Even with R-tree pre-filtering, this is still expensive for coastal paths
        // - Coastal navigation (common use case) may trigger many intersection tests
        // 
        // FUTURE OPTIMIZATION: Consider segment-level intersection testing
        // Current approach: Use existing proven intersection logic without changes
        double lat1_copy = lat1, lon1_copy = lon1, lat2_copy = lat2, lon2_copy = lon2;
        return m_originalChart->CrossesLand(lat1_copy, lon1_copy, lat2_copy, lon2_copy);
    }
    
    LLBBox GetBoundingBox() const override {
        // DATASET BOUNDS: Return bounds for ALL polygons in this shapefile/chart
        // For OSMSHP: Typically covers one geographic tile (1×1° to 10×10°)
        return m_originalChart->GetBoundingBox();
    }
    
    std::vector<LLBBox> GetSegmentBounds() const override {
        // UNIFIED SUBDIVISION: Delegate to single algorithm - highest priority for optimization
        // OSMSHP contains the largest polygons in maritime data, making subdivision critical
        return UnifiedPolygonSubdivision::SubdivideGeometry(*this);
    }
    
    // GEOMETRY ACCESS INTERFACE: Support unified subdivision algorithm
    size_t GetContourCount() const override {
        // Access shapefile contour data - requires investigation of ShapeBaseChart internals
        const auto& contourLists = GetShapefileContours(*m_originalChart);
        size_t totalContours = 0;
        for (const auto& contourList : contourLists) {
            totalContours += contourList.size();
        }
        return totalContours;
    }
    
    size_t GetContourVertexCount(size_t contourIndex) const override {
        // Navigate to specific contour across all shapefile contour lists
        const auto& contourLists = GetShapefileContours(*m_originalChart);
        size_t currentIndex = 0;
        
        for (const auto& contourList : contourLists) {
            if (currentIndex + contourList.size() > contourIndex) {
                size_t localIndex = contourIndex - currentIndex;
                return contourList[localIndex].size();
            }
            currentIndex += contourList.size();
        }
        return 0;
    }
    
    wxRealPoint GetContourVertex(size_t contourIndex, size_t vertexIndex) const override {
        // Navigate to specific vertex across all shapefile contour lists
        const auto& contourLists = GetShapefileContours(*m_originalChart);
        size_t currentIndex = 0;
        
        for (const auto& contourList : contourLists) {
            if (currentIndex + contourList.size() > contourIndex) {
                size_t localIndex = contourIndex - currentIndex;
                if (vertexIndex >= contourList[localIndex].size()) return wxRealPoint(0, 0);
                
                const wxRealPoint& vertex = contourList[localIndex][vertexIndex];
                return vertex; // Already in lat/lon format
            }
            currentIndex += contourList.size();
        }
        return wxRealPoint(0, 0);
    }

private:
    // Helper to access shapefile contour data - requires implementation
    // IMPLEMENTATION NOTE: This requires investigation of ShapeBaseChart internal structure
    std::vector<contour_list> GetShapefileContours(const ShapeBaseChart& chart) const;
    
    const void* GetNativeGeometry() const override {
        return m_originalChart;  // Direct access to existing ShapeBaseChart
    }
    
    std::string GetNativeType() const override { return "ShapeBaseChart"; }
    std::string GetDataSourceName() const override { return "OSMSHP"; }
};

// Extend existing S-57 chart structures to implement the interface
class S57ChartAdapter : public ICoastlineGeometry {
private:
    s57chart* m_originalChart;  // Reference to existing data - NO COPY
    
public:
    explicit S57ChartAdapter(s57chart* chart) : m_originalChart(chart) {}
    
    bool IntersectsLine(double lat1, double lon1, double lat2, double lon2) const override {
        // Implement using existing S-57 LNDARE object traversal
        return CheckS57LandIntersection(m_originalChart, lat1, lon1, lat2, lon2);
    }
    
    std::vector<LLBBox> GetSegmentBounds() const override {
        // ADAPTIVE SUBDIVISION for S-57/ENC LNDARE (land area) objects
        // S-57 may have large land polygons (islands, continents) that need subdivision
        return SubdivideS57LandAreas(*m_originalChart);
    }

private:
    // Extract and subdivide S-57 LNDARE polygons for spatial indexing
    std::vector<LLBBox> SubdivideS57LandAreas(const s57chart& chart) const {
        std::vector<LLBBox> segmentBounds;
        
        // Iterate through S-57 LNDARE (land area) objects
        // Note: This requires investigation of s57chart structure for LNDARE access
        
        const auto& landObjects = GetS57LandObjects(chart);
        
        for (const auto& landObject : landObjects) {
            // Convert OGR geometry to our subdivision format
            auto contours = ConvertOGRGeometryToContours(landObject.geometry);
            
            for (const auto& contour : contours) {
                // Apply same subdivision strategy as other sources
                auto subdivisionBounds = SubdivideContourForIndexing(contour);
                segmentBounds.insert(segmentBounds.end(), 
                                   subdivisionBounds.begin(), subdivisionBounds.end());
            }
        }
        
        return segmentBounds;
    }
    
    // Subdivision logic shared with other adapters
    std::vector<LLBBox> SubdivideContourForIndexing(const contour& polygon) const {
        std::vector<LLBBox> segmentBounds;
        
        LLBBox totalBounds = CalculateContourBounds(polygon);
        double maxDimension = std::max(totalBounds.GetLonRange(), totalBounds.GetLatRange());
        
        const double SUBDIVISION_THRESHOLD = 1.0;  // degrees - may need different threshold for ENC
        
        if (maxDimension <= SUBDIVISION_THRESHOLD || polygon.size() < 10) {
            segmentBounds.push_back(totalBounds);
            return segmentBounds;
        }
        
        // Sliding window subdivision
        const size_t VERTICES_PER_SEGMENT = 50;
        const size_t OVERLAP_VERTICES = 5;
        
        for (size_t start = 0; start < polygon.size(); start += (VERTICES_PER_SEGMENT - OVERLAP_VERTICES)) {
            size_t end = std::min(start + VERTICES_PER_SEGMENT, polygon.size());
            
            LLBBox segmentBox;
            for (size_t i = start; i < end; ++i) {
                segmentBox.Expand(polygon[i].y, polygon[i].x);
            }
            
            segmentBounds.push_back(segmentBox);
            if (end >= polygon.size()) break;
        }
        
        return segmentBounds;
    }
    
    // Helper methods requiring S-57 implementation details
    struct S57LandObject {
        OGRGeometry* geometry;
        // Additional S-57 attributes if needed
    };
    
    std::vector<S57LandObject> GetS57LandObjects(const s57chart& chart) const;
    std::vector<contour> ConvertOGRGeometryToContours(OGRGeometry* geometry) const;
    
    const void* GetNativeGeometry() const override {
        return m_originalChart;  // Direct access to existing s57chart
    }
    
    std::string GetNativeType() const override { return "s57chart"; }
    std::string GetDataSourceName() const override { return "S-57/ENC"; }
};
```

### Zero-Copy Spatial Indexing Strategy

**Memory-Efficient Indexing**: Build R-tree indices over existing data without duplication:

```cpp
// Spatial index that references existing data structures
class ZeroCopyCoastlineSpatialIndex {
public:
    struct IndexEntry {
        LLBBox bounds;                    // Small bounding box - only thing we copy
        ICoastlineGeometry* geometry;     // Reference to existing data - NO COPY
        uint32_t segmentId;               // Index into existing structure
    };
    
    // Build index from existing data structures without copying geometry
    void IndexExistingDataSources() {
        // Index existing GSHHS cells
        for (auto* gshhsCell : GetExistingGshhsCells()) {
            auto adapter = std::make_unique<GshhsPolyCellAdapter>(gshhsCell);
            IndexGeometry(adapter.get());
            m_adapters.push_back(std::move(adapter));  // Keep adapter alive, not the data
        }
        
        // Index existing shapefile charts
        for (auto* shapeChart : GetExistingShapeCharts()) {
            auto adapter = std::make_unique<ShapeBaseChartAdapter>(shapeChart);
            IndexGeometry(adapter.get());
            m_adapters.push_back(std::move(adapter));
        }
        
        // Index existing S-57 charts
        for (auto* s57Chart : GetExistingS57Charts()) {
            auto adapter = std::make_unique<S57ChartAdapter>(s57Chart);
            IndexGeometry(adapter.get());
            m_adapters.push_back(std::move(adapter));
        }
    }
    
    // Fast spatial query using existing data - no copies
    // 
    // TWO-STAGE PERFORMANCE OPTIMIZATION:
    // Stage 1: R-tree spatial pre-filtering (O(log n))
    // Stage 2: Precise intersection testing on candidates (O(k) where k << n)
    //
    // TOTAL COMPLEXITY: O(log n + k) vs original O(n) linear search
    bool FastCrossesLand(double lat1, double lon1, double lat2, double lon2) {
        LLBBox queryBounds = LLBBox::FromTwoPoints(lat1, lon1, lat2, lon2);
        
        // STAGE 1: Fast spatial pre-filtering using R-tree index
        // Query subdivided bounding boxes to find potential intersections
        // This eliminates most coastline polygons without expensive intersection testing
        auto candidates = m_rtree.Query(queryBounds);
        
        if (candidates.empty()) {
            return false;  // ULTRA-FAST PATH: No coastlines near trajectory
        }
        
        // STAGE 2: Precise intersection testing using existing algorithms
        // Only test candidates that survived spatial pre-filtering
        // DUPLICATE ELIMINATION: Same geometry may appear multiple times due to subdivision
        std::unordered_set<ICoastlineGeometry*> testedGeometries;
        
        for (const auto& entry : candidates) {
            // Skip if we've already tested this geometry object
            if (testedGeometries.count(entry.geometry) > 0) {
                continue;
            }
            testedGeometries.insert(entry.geometry);
            
            // Delegate to existing CrossesLand()/crossing1() implementations
            // This maintains functional correctness while benefiting from spatial acceleration
            if (TestCandidateIntersection(entry, lat1, lon1, lat2, lon2)) {
                return true;
            }
        }
        return false;
    }
    
private:
    RTree<IndexEntry> m_rtree;  // Only stores small IndexEntry structs
    std::vector<std::unique_ptr<ICoastlineGeometry>> m_adapters;  // Adapters, not data
    
    void IndexGeometry(ICoastlineGeometry* geometry) {
        // Index subdivided segments for better spatial discrimination
        auto bounds = geometry->GetSegmentBounds();
        for (size_t i = 0; i < bounds.size(); ++i) {
            IndexEntry entry{bounds[i], geometry, static_cast<uint32_t>(i)};
            m_rtree.Insert(bounds[i], entry);
        }
    }
    
    // STAGE 2 COORDINATOR: Bridge between R-tree results and existing intersection algorithms
    bool TestCandidateIntersection(const IndexEntry& candidate, 
                                 double lat1, double lon1, double lat2, double lon2) {
        // IMPORTANT: Test against ENTIRE original geometry, not just the subdivided segment
        // 
        // SUBDIVISION CONTEXT:
        // - GetSegmentBounds() subdivided large polygons into small bounding boxes
        // - R-tree identified which subdivided boxes intersect the query
        // - But intersection testing must use the COMPLETE original polygon
        // 
        // DELEGATES TO: existing CrossesLand()/crossing1() implementations
        // PRESERVES: functional correctness of existing proven algorithms
        return candidate.geometry->IntersectsLine(lat1, lon1, lat2, lon2);
    }
    
    // Get references to existing data structures - no ownership transfer
    std::vector<GshhsPolyCell*> GetExistingGshhsCells();
    std::vector<ShapeBaseChart*> GetExistingShapeCharts();
    std::vector<s57chart*> GetExistingS57Charts();
};
```

### Complete Two-Stage Spatial Query Architecture

**ARCHITECTURE SUMMARY**: The R-tree spatial indexing works in conjunction with existing intersection algorithms, not as a replacement:

```cpp
// COMPLETE FLOW: How spatial acceleration preserves existing algorithms
// 
// 1. BUILD PHASE (one-time setup):
//    a) GetSegmentBounds() subdivides large polygons into ~1° bounding boxes
//    b) R-tree indexes these small bounding boxes (not the original geometry)
//    c) Original polygon data remains unchanged and uncopied
//
// 2. QUERY PHASE (every CrossesLand call):
//    a) R-tree.Query() finds subdivided boxes that intersect trajectory (O(log n))
//    b) Gather unique geometry objects from surviving candidates
//    c) IntersectsLine() tests each geometry using existing algorithms (O(k))
//    d) Return true if any geometry intersects
//
// PERFORMANCE TRANSFORMATION:
// - Original: Test every polygon in dataset (O(n))
// - New: Test only nearby polygons found by spatial index (O(log n + k))
// - Typical case: k (nearby polygons) << n (total polygons)

class CompleteSpacialQueryExample {
public:
    static bool DemonstrateQueryFlow(double lat1, double lon1, double lat2, double lon2) {
        
        // EXAMPLE: Query near San Francisco Bay
        
        // STAGE 1: R-tree spatial pre-filtering
        // Query finds subdivided bounding boxes that intersect trajectory
        // WITHOUT subdivision: Would return "entire Pacific coast polygon" (massive)
        // WITH subdivision: Returns only ~5 bounding boxes near SF Bay (~555km total)
        auto candidates = g_spatialIndex.Query(trajectoryBounds);
        
        // STAGE 2: Precise intersection testing
        // Test intersection against complete original polygons (not subdivided pieces)
        for (const auto& candidate : candidates) {
            // This calls existing CrossesLand()/crossing1() - no changes to intersection logic
            if (candidate.geometry->IntersectsLine(lat1, lon1, lat2, lon2)) {
                return true;  // Found intersection using proven existing algorithm
            }
        }
        
        return false;
    }
};
```

### Zero-Copy Interface Implementation Strategy

**Core Philosophy**: Make existing OpenCPN data structures implement spatial interfaces directly, eliminating the need for data copying or conversion.

#### Extending Existing Data Structures

Rather than creating new data structures, extend existing ones to implement the `ICoastlineGeometry` interface:

```cpp
// Option A: Extend existing classes (if possible without breaking compatibility)
class GshhsPolyCell : public ICoastlineGeometry {
    // Existing GSHHS implementation remains unchanged
    // Add interface methods that delegate to existing functionality
public:
    // Existing methods remain unchanged...
    std::vector<wxLineF> *getCoasts() { return &coasts; }
    contour_list &getPoly1() { return poly1; }
    
    // New interface methods - no data duplication
    bool IntersectsLine(double lat1, double lon1, double lat2, double lon2) const override {
        return crossing1(/* convert parameters to existing format */);
    }
    
    std::vector<LLBBox> GetSegmentBounds() const override {
        // Calculate bounds from existing contour_list data
        std::vector<LLBBox> bounds;
        for (const auto& contour : poly1) {
            bounds.push_back(CalculateContourBounds(contour));
        }
        return bounds;
    }
    
    const void* GetNativeGeometry() const override { return &poly1; }
    std::string GetNativeType() const override { return "contour_list"; }
};

// Option B: Non-intrusive adapter pattern (safer for existing code)
class GshhsAdapter {
private:
    GshhsPolyCell* m_cell;  // Reference to existing data
    
public:
    explicit GshhsAdapter(GshhsPolyCell* cell) : m_cell(cell) {}
    
    // Implement ICoastlineGeometry interface by delegating to existing methods
    bool IntersectsLine(double lat1, double lon1, double lat2, double lon2) const {
        return m_cell->crossing1(/* parameters */);
    }
    
    // Access existing data structures directly
    const contour_list& GetExistingContours() const { return m_cell->getPoly1(); }
    const std::vector<wxLineF>& GetExistingCoasts() const { return *m_cell->getCoasts(); }
};
```

#### Interface-Based Spatial Indexing

**Universal Indexing**: Single indexing system works with all data types through interfaces:

```cpp
class UniversalCoastlineIndex {
public:
    // Index any object that implements ICoastlineGeometry
    void AddGeometry(ICoastlineGeometry* geometry) {
        // Extract bounding boxes without copying underlying data
        auto segmentBounds = geometry->GetSegmentBounds();
        
        for (size_t i = 0; i < segmentBounds.size(); ++i) {
            IndexEntry entry{segmentBounds[i], geometry, static_cast<uint32_t>(i)};
            m_rtree.Insert(segmentBounds[i], entry);
        }
    }
    
    // Works with any geometry type - GSHHS, OSMSHP, S-57, etc.
    bool CrossesLand(double lat1, double lon1, double lat2, double lon2) {
        auto candidates = m_rtree.Query(LLBBox::FromTwoPoints(lat1, lon1, lat2, lon2));
        
        for (const auto& candidate : candidates) {
            // Polymorphic call - works with any implementation
            if (candidate.geometry->IntersectsLine(lat1, lon1, lat2, lon2)) {
                return true;
            }
        }
        return false;
    }
    
private:
    struct IndexEntry {
        LLBBox bounds;              // Only copy small bounding box
        ICoastlineGeometry* geometry; // Reference to existing data
        uint32_t segmentId;
    };
    
    RTree<IndexEntry> m_rtree;
};

// Factory for creating appropriate adapters
class GeometryAdapterFactory {
public:
    static std::unique_ptr<ICoastlineGeometry> CreateAdapter(ChartBase* chart) {
        // Detect chart type and create appropriate adapter
        if (auto* gshhsChart = dynamic_cast<GSHHSChart*>(chart)) {
            return std::make_unique<GshhsAdapter>(gshhsChart->getCurrentCell());
        }
        if (auto* shapeChart = dynamic_cast<ShapeBaseChart*>(chart)) {
            return std::make_unique<ShapeBaseChartAdapter>(shapeChart);
        }
        if (auto* s57Chart = dynamic_cast<s57chart*>(chart)) {
            return std::make_unique<S57ChartAdapter>(s57Chart);
        }
        return nullptr;
    }
};
```

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

**Maintain Current API**: The existing `PlugIn_GSHHS_CrossesLand()` function must continue to work unchanged for GSHHS data.

```cpp
// Existing API preserved
extern "C" bool PlugIn_GSHHS_CrossesLand(double lat1, double lon1, 
                                          double lat2, double lon2) {
    // Route through new unified system but maintain exact same behavior
    return g_UnifiedSpatialIndex->CrossesLand(lat1, lon1, lat2, lon2);
}
```

**Gradual Migration Path**:

A new API with a high-performance backend will be implemented.

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

#### 3. Zero-Copy Memory Management Integration

**Eliminate Data Duplication**: Build upon existing OpenCPN data structures without copying:

```cpp
class ZeroCopyUnifiedSpatialCache {
private:
    // Only cache adapters and small metadata - never duplicate geometry data
    std::unordered_map<std::string, std::weak_ptr<ICoastlineGeometry>> m_adapterCache;
    std::chrono::time_point<std::chrono::steady_clock> m_lastCleanup;
    
public:
    // Respect existing memory pressure handling without additional memory burden
    void OnMemoryPressure() {
        // Clean up adapter references, but don't affect original data
        CleanupExpiredAdapters();
        // Original data structures handle their own memory management
    }
    
    // Coordinate with existing chart systems - no additional memory usage
    ICoastlineGeometry* GetGeometryAdapter(ChartBase* chart) {
        if (auto* s57Chart = dynamic_cast<s57chart*>(chart)) {
            // Return adapter that references existing S-57 data - no copying
            return GetOrCreateS57Adapter(s57Chart);
        }
        if (auto* shapeChart = dynamic_cast<ShapeBaseChart*>(chart)) {
            // Return adapter that references existing shapefile data - no copying  
            return GetOrCreateShapeAdapter(shapeChart);
        }
        return nullptr;
    }
    
    // Memory usage analysis
    size_t GetMemoryFootprint() const {
        // Should be minimal - only adapters and bounding boxes, no geometry data
        return m_adapterCache.size() * sizeof(std::weak_ptr<ICoastlineGeometry>);
    }
    
private:
    ICoastlineGeometry* GetOrCreateS57Adapter(s57chart* chart);
    ICoastlineGeometry* GetOrCreateShapeAdapter(ShapeBaseChart* chart);
};

// Integration with existing OpenCPN data lifecycle
class ExistingDataIntegration {
public:
    // Hook into existing chart loading - no additional memory allocation
    static void OnChartLoaded(ChartBase* chart) {
        // Create adapter when chart is loaded, but don't copy data
        auto* adapter = CreateAdapterForChart(chart);
        if (adapter) {
            g_SpatialIndex.AddGeometry(adapter);  // Index references, not data
        }
    }
    
    // Hook into existing chart unloading - clean up references
    static void OnChartUnloaded(ChartBase* chart) {
        // Remove adapter references when chart is unloaded
        g_SpatialIndex.RemoveGeometry(chart);
        // Original chart data cleanup handled by existing code
    }
    
    // Work with existing OpenCPN memory management
    static ICoastlineGeometry* CreateAdapterForChart(ChartBase* chart) {
        // Factory pattern - create appropriate adapter without copying data
        if (auto* gshhsReader = GetGshhsReader()) {
            return new GshhsPolyCellAdapter(gshhsReader->getCurrentCell());
        }
        // ... other chart types
        return nullptr;
    }
};
```

**Key Zero-Copy Principles**:

1. **Adapter Pattern**: Create lightweight adapters over existing data structures
2. **Reference-Only Indexing**: R-tree stores only bounding boxes and references, not geometry data
3. **Lifecycle Integration**: Hook into existing chart loading/unloading without additional memory burden
4. **Native Access**: Provide direct access to original data structures when needed
5. **Memory Monitoring**: Track adapter overhead separately from original data memory usage

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

#### 5. High-Performance Zero-Copy Spatial Indexing Implementation

**The Core Performance Challenge**: Address the O(n) linear search bottleneck without data duplication:

```cpp
class ZeroCopyPerformanceOptimizedQuery {
public:
    // Replace O(n) linear polygon search with O(log n) spatial indexing - no data copying
    static bool FastCrossesLand(double lat1, double lon1, double lat2, double lon2,
                               const ZeroCopyCoastlineSpatialIndex& spatialIndex) {
        
        LLBBox trajectoryBounds = LLBBox::FromTwoPoints(lat1, lon1, lat2, lon2);
        
        // Step 1: Fast spatial pre-filtering using R-tree (O(log n))
        // Only small IndexEntry structs are traversed - no geometry data
        auto candidates = spatialIndex.QueryCandidates(trajectoryBounds);
        
        if (candidates.empty()) {
            return false;  // No coastlines near trajectory - ultra-fast path
        }
        
        // Step 2: Test intersection using existing geometry implementations
        // Delegates to existing CrossesLand() methods - no data copying
        for (const auto& candidate : candidates) {
            if (candidate.geometry->IntersectsLine(lat1, lon1, lat2, lon2)) {
                return true;
            }
        }
        return false;
    }
    
    // Performance monitoring with memory efficiency validation
    static void ValidateZeroCopyPerformance() {
        // Measure memory usage - should be minimal overhead over original data
        size_t baselineMemory = GetExistingChartMemoryUsage();
        size_t indexedMemory = GetIndexedChartMemoryUsage();
        size_t overhead = indexedMemory - baselineMemory;
        
        // Validate that overhead is minimal (only bounding boxes + adapters)
        assert(overhead < baselineMemory * 0.05);  // <5% memory overhead
        
        // A/B test performance improvements
        MeasureQueryPerformance();
    }
    
private:
    static ZeroCopyCoastlineSpatialIndex m_spatialIndex;
};

// Zero-copy indexing strategy for existing data structures
class ZeroCopyCoastlineIndexBuilder {
public:
    // Build indices over existing data without copying geometry
    void BuildIndexForExistingData() {
        // Hook into existing GSHHS system
        IndexExistingGshhsData();
        
        // Hook into existing shapefile system  
        IndexExistingShapefileData();
        
        // Hook into existing S-57 system
        IndexExistingS57Data();
    }
    
    // Leverage existing GSHHS data structures
    void IndexExistingGshhsData() {
        // Access existing GSHHSChart instances without copying
        for (auto* gshhsChart : GetLoadedGshhsCharts()) {
            auto adapter = std::make_unique<GshhsPolyCellAdapter>(gshhsChart->getCurrentCell());
            
            // Only extract bounding boxes for R-tree - don't copy vertices
            auto bounds = adapter->GetSegmentBounds();
            for (const auto& bound : bounds) {
                m_spatialIndex.Insert(bound, adapter.get());
            }
            
            m_adapters.push_back(std::move(adapter));  // Keep adapter alive
        }
    }
    
    void IndexExistingShapefileData() {
        // Access existing ShapeBaseChart contour_list structures directly
        for (auto* shapeChart : GetLoadedShapeCharts()) {
            auto adapter = std::make_unique<ShapeBaseChartAdapter>(shapeChart);
            
            // Build spatial index over existing polygon representations - no copying
            auto bounds = adapter->GetSegmentBounds();
            for (const auto& bound : bounds) {
                m_spatialIndex.Insert(bound, adapter.get());
            }
            
            m_adapters.push_back(std::move(adapter));
        }
    }
    
    void IndexExistingS57Data() {
        // Access existing S-57 LNDARE object data directly
        for (auto* s57Chart : GetLoadedS57Charts()) {
            auto adapter = std::make_unique<S57ChartAdapter>(s57Chart);
            
            // Use existing OGR geometry bounding boxes - no data duplication
            auto bounds = adapter->GetSegmentBounds();
            for (const auto& bound : bounds) {
                m_spatialIndex.Insert(bound, adapter.get());
            }
            
            m_adapters.push_back(std::move(adapter));
        }
    }
    
private:
    ZeroCopyCoastlineSpatialIndex m_spatialIndex;
    std::vector<std::unique_ptr<ICoastlineGeometry>> m_adapters;
    
    // Get references to existing loaded data - no ownership transfer
    std::vector<GSHHSChart*> GetLoadedGshhsCharts();
    std::vector<ShapeBaseChart*> GetLoadedShapeCharts();
    std::vector<s57chart*> GetLoadedS57Charts();
};
```

**Key Zero-Copy Performance Optimizations**:

1. **Reference-Only Indexing**: R-tree stores only small bounding boxes and pointers, not geometry data
2. **Existing Data Reuse**: Access existing `contour_list`, `OGRPolygon`, and GSHHS structures directly  
3. **Minimal Memory Overhead**: <5% memory increase over baseline chart data
4. **Adapter Lifecycle**: Lightweight adapters track existing data lifecycle automatically
5. **Performance Validation**: Measure both speed improvements and memory efficiency
6. **Backward Compatibility**: Preserve exact behavior of existing `PlugIn_GSHHS_CrossesLand()` without data changes

#### 6. Future Plugin Integration

**Note**: Plugin spatial data integration requires further analysis of the plugin architecture and is deferred to future tasks. Current focus is on the four well-understood data sources: GSHHS, OSMSHP, S-57/ENC, and generic Shapefiles.

### Practical Zero-Copy Implementation Strategy

#### Phase 1: Interface-Based Spatial Indexing with Zero Data Duplication

**Immediate Goal**: Create high-performance spatial indexing over existing data structures without copying any geometry data.

**Zero-Copy Approach**:

1. **Non-Intrusive Adapters**: Create lightweight adapter classes that implement `ICoastlineGeometry` over existing structures
2. **Reference-Only Indexing**: Build R-tree indices that store only bounding boxes and references to existing data
3. **Existing API Preservation**: Keep `PlugIn_GSHHS_CrossesLand()` unchanged, route through spatial index for acceleration
4. **Memory Efficiency Validation**: Ensure <5% memory overhead over baseline chart data

**Implementation Steps**:

```cpp
// Step 1: Create adapters for existing data structures
auto gshhsAdapter = std::make_unique<GshhsAdapter>(existingGshhsCell);
auto osmshpAdapter = std::make_unique<ShapeBaseChartAdapter>(existingShapeChart);
auto s57Adapter = std::make_unique<S57ChartAdapter>(existingS57Chart);

// Step 2: Build spatial index over existing data - no copying
UniversalCoastlineIndex spatialIndex;
spatialIndex.AddGeometry(gshhsAdapter.get());  // References existing data
spatialIndex.AddGeometry(osmshpAdapter.get());  // References existing data
spatialIndex.AddGeometry(s57Adapter.get());    // References existing data

// Step 3: Fast queries using existing data through spatial acceleration
bool crossesLand = spatialIndex.CrossesLand(lat1, lon1, lat2, lon2);
```

**Implementation Priority**:

- **OSMSHP: HIGHEST PRIORITY** - Fix unacceptable performance by indexing existing `ShapeBaseChart` data
- **GSHHS**: Add to unified system for consistency, minimal performance improvement expected
- **S-57/ENC**: Test performance and add indexing if needed

#### Phase 2: Unified API with Data Source Selection

**Goal**: Provide unified coastline access while maintaining zero data duplication.

**Selection Strategies**:

```cpp
class ZeroCopyCoastlineAPI {
public:
    enum class SelectionMode {
        FASTEST_AVAILABLE,      // Use best performing source (considers index effectiveness)
        HIGHEST_RESOLUTION,     // Use most detailed source available
        SPECIFIC_SOURCE,        // Use only specified source type
        ALL_SOURCES_CONSENSUS   // Check multiple sources (for safety-critical apps)
    };
    
    // Unified API - delegates to existing implementations via adapters
    bool CrossesLand(double lat1, double lon1, double lat2, double lon2, 
                    SelectionMode mode = SelectionMode::FASTEST_AVAILABLE) {
        // Select appropriate adapter based on mode and regional availability
        auto* selectedAdapter = SelectAdapter(lat1, lon1, lat2, lon2, mode);
        
        // Use spatial index for acceleration - no data copying
        return m_spatialIndex.CrossesLandWithAdapter(lat1, lon1, lat2, lon2, selectedAdapter);
    }
    
private:
    UniversalCoastlineIndex m_spatialIndex;
    std::vector<std::unique_ptr<ICoastlineGeometry>> m_adapters;  // References only
    
    ICoastlineGeometry* SelectAdapter(double lat, double lon, double lat2, double lon2, SelectionMode mode);
};
```

#### Phase 3: Extended Feature Types (Future)

**Scope**: After coastline performance is solved, extend to other spatial features using the same indexing approach:

- Depth contours (DEPARE, DEPCNT from S-57)
- Navigation hazards (OBSTRN, UWTROC, WRECKS)
- Restricted areas (traffic separation zones, military areas)

#### Phase 4: Advanced Intersection Performance Optimization (Future)

**IDENTIFIED PERFORMANCE BOTTLENECK**: Even with R-tree spatial acceleration, `IntersectsLine()` can be expensive:

**The Problem**:

- Coastal navigation paths trigger many intersection tests (common use case)
- Single OSMSHP polygons contain 10+ million vertices
- Current approach tests entire polygon even after spatial pre-filtering
- May still result in unacceptable performance for real-time coastal pathfinding

**Future Three-Stage Optimization Strategy**:

```cpp
// STAGE 1: R-tree spatial pre-filtering (current implementation)
// STAGE 2: Segment-level intersection testing (future optimization)
// STAGE 3: Full polygon intersection testing (fallback)

class AdvancedIntersectionOptimization {
public:
    // FUTURE: Segment-aware intersection testing
    bool IntersectsLineWithSegmentOptimization(
        double lat1, double lon1, double lat2, double lon2,  
        const IndexEntry& candidate) {
        
        // Use subdivision segment information to test only relevant polygon parts
        uint32_t segmentId = candidate.segmentId;
        
        // Test intersection against subdivision segment boundaries first
        if (!candidate.bounds.IntersectsLine(lat1, lon1, lat2, lon2)) {
            return false; // Fast rejection using bounding box
        }
        
        // Test intersection against only the relevant polygon segment
        return TestPolygonSegmentIntersection(candidate.geometry, segmentId, 
                                            lat1, lon1, lat2, lon2);
    }
    
private:
    // Test intersection against subdivision segment, not entire polygon
    bool TestPolygonSegmentIntersection(ICoastlineGeometry* geometry,
                                      uint32_t segmentId,
                                      double lat1, double lon1, double lat2, double lon2);
};
```

**Implementation Priority**:

- **Phase 1**: Focus on R-tree spatial acceleration (current design)
- **Phase 2**: Measure real-world performance with coastal navigation paths
- **Phase 3**: If needed, implement segment-level intersection optimization

**Design Principle**: Preserve functional correctness while incrementally improving performance

### Technical Decisions and Rationale

#### 1. Adaptive Polygon Subdivision Strategy

**The Core Problem**: Maritime data sources optimize for storage and display, not spatial queries:

- **OSMSHP**: Single polygon for entire Pacific West Coast (thousands of kilometers)
- **GSHHS**: Continental landmasses as single polygons  
- **S-57/ENC**: Large island or coastal features as single LNDARE objects

**Naive Spatial Indexing Failure**: Simply creating one bounding box per polygon results in:

- Massive bounding boxes covering entire continents or ocean basins
- R-tree queries return huge polygons as "candidates" for any nearby query
- No performance improvement - still testing intersection against enormous polygons

**Adaptive Subdivision Solution**:

```cpp
// SUBDIVISION THRESHOLD: If polygon spans > 1.0 degrees, subdivide
const double SUBDIVISION_THRESHOLD = 1.0;  // degrees (roughly 111km at equator)

// SLIDING WINDOW APPROACH: Create overlapping segments  
const size_t VERTICES_PER_SEGMENT = 50;  // ~50 vertices per spatial index segment
const size_t OVERLAP_VERTICES = 5;       // Overlap to ensure geometric continuity
```

**Key Benefits**:

1. **Spatial Discrimination**: Large coastlines broken into ~1° segments for efficient R-tree filtering
2. **Geometric Continuity**: Overlapping segments prevent gaps in intersection detection  
3. **Adaptive Granularity**: Small polygons (<1° or <10 vertices) remain as single segments
4. **Zero Data Duplication**: Only bounding boxes are subdivided, original geometry stays intact

**Performance Impact**:

- **Before**: Query against Pacific coast returns entire continent as candidate
- **After**: Query returns only nearby ~1° coastal segments as candidates
- **Expected Improvement**: 10-100x reduction in candidate polygon size for intersection testing

#### 2. Why Leverage Existing CrossesLand() Functions?

**Functional Correctness**: The existing `ShapeBaseChart::CrossesLand()` and `PlugIn_GSHHS_CrossesLand()` functions are **functionally correct** - they produce accurate land/water determinations. The problem is purely performance, not correctness.

**Risk Reduction**: Rather than reimplement complex polygon intersection algorithms, build spatial indexing **around** existing proven code to accelerate it.

**Faster Implementation**: Creating efficient indices over existing data structures is faster than rebuilding geometry processing from scratch.

#### 2. Why Focus on OSMSHP Performance First?

**Known Performance Problem**: OSMSHP `CrossesLand()` has **unacceptable performance** despite functional correctness - this is the immediate pain point.

**High Resolution Data**: OSMSHP provides excellent resolution coastline data in many areas, but can't be used due to performance limitations.

**Proven Approach**: Spatial indexing (R-trees) is a well-understood solution for accelerating polygon intersection queries.

#### 3. Why Application-Driven Source Selection?

**No Universal "Best" Source**:

- **OSMSHP**: Crowdsourced quality varies by region, but high resolution where good
- **ENC**: Official data but limited coverage and potentially outdated
- **GSHHS**: Global coverage but lower resolution

**Different Use Cases Need Different Sources**:

- **Real-time pathfinding**: Needs fastest performance (might prefer GSHHS)
- **Route planning**: Wants highest resolution available (might prefer OSMSHP/ENC)
- **Safety validation**: Might want to check multiple sources for consensus

### Next Steps for Task 1.3: High-Performance Spatial Indexing Implementation

This abstraction design provides the foundation for implementing **efficient spatial indices** to solve the OSMSHP performance problem:

#### Task 1.3 Primary Objectives - Zero-Copy Implementation

1. **Zero-Copy R-tree Spatial Index Implementation**:
   - Create spatial acceleration structure over existing `contour_list`, `OGRPolygon`, and GSHHS data
   - Target 10-100x performance improvement for OSMSHP `CrossesLand()` queries **without data duplication**
   - Preserve exact functional behavior of existing implementations
   - Maintain <5% memory overhead over baseline chart data

2. **Interface-Based Multi-Source Query Optimization**:
   - Implement `ICoastlineGeometry` interfaces on existing data structures (adapter pattern)
   - Add data source selection strategies that work through adapters, not data copies
   - Enable unified plugin API: `PlugIn_UnifiedCrossesLand(lat1, lon1, lat2, lon2, source_priority)`
   - Zero data duplication across all coastline data sources

3. **Zero-Copy Performance Validation Framework**:
   - A/B test indexed vs non-indexed performance with memory usage monitoring
   - Validate that spatial indexing adds minimal memory overhead
   - Measure query performance improvements for weather routing and pathfinding plugins
   - Ensure functional correctness is preserved and no data is duplicated

4. **Non-Intrusive Integration with Existing Systems**:
   - Use adapter pattern to add interfaces to existing structures without modification
   - Leverage existing `typedef std::vector<wxRealPoint> contour`, `ShapeBaseChart`, and `s57chart` data
   - Hook into existing chart loading/unloading lifecycle without memory duplication
   - Maintain backward compatibility with current `PlugIn_GSHHS_CrossesLand()` API

#### Expected Performance Outcomes

- **OSMSHP**: Unacceptable performance → Fast queries **with zero data duplication** (primary goal)
- **S-57/ENC**: Unknown performance → Measured and optimized if needed **without copying geometry data**
- **GSHHS**: Already acceptable → Integrated into unified API **through adapters, not copies**
- **Memory Usage**: <5% increase over baseline due to indexing overhead only
- **Overall**: Enable weather routing and pathfinding plugins to use highest resolution coastline data available **without memory duplication penalty**

The spatial indexing implementation will directly address the core performance bottleneck that prevents effective use of high-resolution coastline data in OpenCPN's navigation and routing systems.
