# Task 4.1: Replace PlugIn_GSHHS_CrossesLand Implementation

**Phase**: Integration & API  
**Dependencies**: Tasks 2.2, 3.2

## Detailed Description

Replace the existing slow PlugIn_GSHHS_CrossesLand function with the new high-performance spatial index, maintaining API compatibility while delivering significant performance improvements.

## Files to Modify

- `gui/src/ocpn_plugin_gui.cpp`
- `gui/src/gshhs.cpp` (add new functions)
- `test/gshhs_performance_test.cpp` (new file)

## Implementation Requirements

### 1. Function Replacement
- Replace GSHHS polygon iteration with R-tree query
- Fall back to distance field when available
- Maintain exact same API signature
- Preserve backward compatibility

### 2. Performance Monitoring
- Track query times and compare with old implementation
- Monitor cache hit rates
- Log performance improvements

### 3. Gradual Rollout
- Feature flag to toggle between old/new implementation
- A/B testing framework for validation
- Performance comparison logging

## Acceptance Criteria

- [ ] API maintains 100% backward compatibility
- [ ] Performance improvement >10x for typical routing queries
- [ ] Results identical to original implementation >99.9% of cases
- [ ] No regression in memory usage during normal operation
- [ ] Weather routing plugin sees significant speedup

## Implementation Status

_This section will track the replacement process and validation results._

## Compatibility Testing

_This section will document testing with existing plugins and chart systems to ensure no regressions._
