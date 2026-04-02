# PMTiles Support - Implementation Summary

## What Was Implemented

This implementation adds full read support for **PMTiles v3** archive format to the CARTO Mobile SDK.

### Files Created

1. **`all/native/datasources/PMTilesTileDataSource.h`** - Header file
2. **`all/native/datasources/PMTilesTileDataSource.cpp`** - Implementation
3. **`all/modules/datasources/PMTilesTileDataSource.i`** - SWIG bindings
4. **`docs/datasources/PMTilesTileDataSource.md`** - Documentation

### Files Modified

- **`all/modules/datasources/MultiTileDataSource.i`** - Added PMTiles import

### Features Implemented

✅ **PMTiles v3 Specification Compliance**
- 127-byte header parsing
- Root directory decoding
- Leaf directory support with caching
- Metadata (JSON) reading
- Hilbert curve tile indexing

✅ **Compression Support**
- Gzip (for internal directories and tiles)
- Uncompressed archives
- Clear error messages for unsupported formats (Brotli, Zstandard)

✅ **Core Functionality**
- Automatic zoom level detection
- Geographic bounds calculation
- Thread-safe file operations
- Efficient directory caching
- Varint decoding

✅ **Integration**
- SWIG bindings for cross-platform support
- Follows SDK patterns (similar to MBTilesTileDataSource)
- Automatic build system integration (glob patterns)

✅ **Quality**
- Memory-safe (return by value)
- Proper thread synchronization
- Comprehensive error handling
- Full documentation

## What Was NOT Implemented

❌ **PMTiles Write/Cache Support**

A writable PMTiles cache data source was NOT implemented because:
1. PMTiles v3 is designed as a read-only format
2. Creating archives requires:
   - Building complete directory structures
   - Hilbert curve tile ordering
   - Compression implementation
   - Incremental update support
3. Goes against PMTiles design philosophy (meant to be pre-generated)

**Alternative**: Use existing `PersistentCacheTileDataSource` (SQLite-based) for caching.

❌ **Brotli and Zstandard Compression**

Not implemented due to missing external libraries:
- Would require adding libbrotli
- Would require adding libzstd

**Workaround**: Create PMTiles files with gzip or no compression:
```bash
pmtiles convert input.mbtiles output.pmtiles --compression=gzip
```

## Usage Example

```cpp
#include "datasources/PMTilesTileDataSource.h"

// Create PMTiles data source
auto pmtiles = std::make_shared<PMTilesTileDataSource>("/path/to/tiles.pmtiles");

// Optional: Add memory caching
auto cached = std::make_shared<MemoryCacheTileDataSource>(pmtiles);

// Use with a layer
auto layer = std::make_shared<VectorTileLayer>(cached, decoder);
mapView->getLayers()->add(layer);

// Access metadata
std::string name = pmtiles->getMetaData("name");
int minZoom = pmtiles->getMinZoom();
int maxZoom = pmtiles->getMaxZoom();
MapBounds bounds = pmtiles->getDataExtent();
```

## Technical Highlights

### Hilbert Curve Implementation
- Proper Z/X/Y to TileID conversion
- Inverse TileID to Z/X/Y conversion
- Quadrant rotation and transformation

### Performance Optimizations
- Root directory: Cached in memory (~16 KB)
- Leaf directories: Loaded on-demand, cached in `std::unordered_map`
- Metadata: Lazy-loaded and cached
- Minimal disk seeks using efficient directory search

### Thread Safety
- `std::recursive_mutex` for all mutable operations
- Immutable header fields accessed without locks
- Cached leaf directories protected by mutex

## Code Quality

All code review feedback addressed:
- ✅ Proper caching strategy
- ✅ No unnecessary locks
- ✅ Thread-safe implementation
- ✅ Memory-safe (no dangling pointers)
- ✅ Clear documentation

## Testing Recommendations

Test with PMTiles files:
1. **Compression**: gzip and uncompressed
2. **Size**: Small (< 10MB) and large (> 100MB with leaf directories)
3. **Tile types**: Vector (MVT), raster (PNG, JPEG, WebP)
4. **Zoom ranges**: Single zoom and multi-zoom
5. **Use cases**: Offline maps, base layers, overlays

## References

- [PMTiles v3 Specification](https://github.com/protomaps/PMTiles/blob/main/spec/v3/spec.md)
- [PMTiles CLI Tools](https://github.com/protomaps/go-pmtiles)
- [Full Documentation](docs/datasources/PMTilesTileDataSource.md)
