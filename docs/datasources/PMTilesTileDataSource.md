# PMTiles Data Source

## Overview

The `PMTilesTileDataSource` class provides support for reading PMTiles v3 archive files in the CARTO Mobile SDK. PMTiles is a single-file archive format for pyramids of tiled data, designed for efficient random access and cloud-optimized serving.

## Features

- ✅ **PMTiles v3 Format**: Full support for PMTiles version 3 specification
- ✅ **Metadata Support**: Read JSON metadata from PMTiles archives
- ✅ **Efficient Tile Access**: Optimized directory lookup using Hilbert curves
- ✅ **Compression Support**: 
  - Gzip compression (both internal and tile compression)
  - Uncompressed archives
  - ⚠️ Brotli and Zstandard support planned for future releases
- ✅ **Tile Types**: Supports all PMTiles tile types (MVT, PNG, JPEG, WebP, AVIF, etc.)
- ✅ **Zoom Level Detection**: Automatic detection of min/max zoom levels from archive
- ✅ **Data Extent**: Automatic bounds calculation from archive metadata

## Usage

### Basic Usage

```cpp
#include "datasources/PMTilesTileDataSource.h"

// Create a PMTiles data source with auto-detected zoom levels
auto dataSource = std::make_shared<PMTilesTileDataSource>("/path/to/tiles.pmtiles");

// Or specify zoom levels explicitly
auto dataSource = std::make_shared<PMTilesTileDataSource>(0, 14, "/path/to/tiles.pmtiles");

// Use with a layer
auto layer = std::make_shared<VectorTileLayer>(dataSource, decoder);
mapView->getLayers()->add(layer);
```

### Accessing Metadata

```cpp
// Get all metadata as JSON string
std::string metadataJson = dataSource->getMetaData();

// Get specific metadata fields
std::string name = dataSource->getMetaData("name");
std::string attribution = dataSource->getMetaData("attribution");
std::string description = dataSource->getMetaData("description");
```

### Getting Archive Information

```cpp
// Get zoom level range
int minZoom = dataSource->getMinZoom();
int maxZoom = dataSource->getMaxZoom();

// Get geographic bounds
MapBounds bounds = dataSource->getDataExtent();
```

## PMTiles File Requirements

### Supported Compression

Currently supported compression formats:
- **None** (0x01): Uncompressed archives
- **Gzip** (0x02): Standard gzip compression

Planned for future support:
- **Brotli** (0x03): Higher compression ratio than gzip
- **Zstandard** (0x04): Fast compression with good ratios

If your PMTiles file uses Brotli or Zstandard compression, you'll need to either:
1. Recreate it with gzip compression
2. Recreate it without compression
3. Wait for future SDK updates with full compression support

### Creating Compatible PMTiles Files

Use the [PMTiles CLI](https://github.com/protomaps/go-pmtiles) to create compatible files:

```bash
# Convert MBTiles to PMTiles with gzip compression
pmtiles convert input.mbtiles output.pmtiles --compression=gzip

# Or without compression for maximum compatibility
pmtiles convert input.mbtiles output.pmtiles --compression=none
```

## Implementation Details

### Hilbert Curve Indexing

PMTiles uses Hilbert curves for tile indexing, which provides better spatial locality than Z-order curves. The implementation includes:
- Efficient Z/X/Y to TileID conversion
- Inverse TileID to Z/X/Y conversion
- Proper Hilbert curve rotation and quadrant handling

### Directory Structure

PMTiles archives contain:
1. **Header** (127 bytes): Archive metadata and offsets
2. **Root Directory**: Primary tile index (max 16 KB compressed)
3. **Metadata**: JSON metadata about the tileset
4. **Leaf Directories**: Optional secondary indexes for large archives
5. **Tile Data**: Actual tile content

The implementation efficiently navigates this structure:
- Root directory is cached in memory
- Leaf directories are loaded on-demand
- Binary search is used for tile lookup

### Thread Safety

The implementation is thread-safe:
- Uses `std::recursive_mutex` for all operations
- File access is properly synchronized
- Safe for use from multiple threads

## Limitations

### Read-Only

PMTiles is a read-only format by design. For caching, use:
- `MemoryCacheTileDataSource` for in-memory caching
- `PersistentCacheTileDataSource` for SQLite-based persistent caching

### Compression Support

Currently limited to gzip compression. Brotli and Zstandard support requires adding:
- Brotli library (libbrotli)
- Zstandard library (libzstd)

These will be added in future releases.

## Error Handling

The data source throws exceptions for:
- Invalid PMTiles files (wrong magic number or version)
- Unsupported compression formats
- File I/O errors
- Corrupted directory structures

All errors are logged to the SDK logger with detailed error messages.

## Performance Considerations

### Memory Usage

- Root directory is cached in memory (typically < 16 KB)
- Metadata is cached on first access
- Leaf directories are loaded on-demand and cached for reuse
- Individual tiles are not cached by this data source

### Disk Access

- Optimized for minimal seeks
- Uses efficient binary search in directories
- Reads exact tile sizes (no over-reading)

### Recommendations

For best performance:
1. Use PMTiles with gzip compression (good balance)
2. Wrap with `MemoryCacheTileDataSource` for frequently accessed tiles
3. Consider zoom-dependent tile loading strategies
4. Pre-warm cache for expected viewing areas

## Examples

### Vector Tiles

```cpp
// Load vector tile PMTiles
auto pmtilesSource = std::make_shared<PMTilesTileDataSource>("/path/to/vector.pmtiles");
auto decoder = std::make_shared<MBVectorTileDecoder>(styleAsset);
auto layer = std::make_shared<VectorTileLayer>(pmtilesSource, decoder);
mapView->getLayers()->add(layer);
```

### Raster Tiles

```cpp
// Load raster tile PMTiles
auto pmtilesSource = std::make_shared<PMTilesTileDataSource>("/path/to/raster.pmtiles");
auto layer = std::make_shared<RasterTileLayer>(pmtilesSource);
mapView->getLayers()->add(layer);
```

### With Caching

```cpp
// PMTiles with memory cache
auto pmtilesSource = std::make_shared<PMTilesTileDataSource>("/path/to/tiles.pmtiles");
auto cachedSource = std::make_shared<MemoryCacheTileDataSource>(pmtilesSource);
auto layer = std::make_shared<VectorTileLayer>(cachedSource, decoder);
mapView->getLayers()->add(layer);
```

## See Also

- [PMTiles Specification](https://github.com/protomaps/PMTiles/blob/main/spec/v3/spec.md)
- [PMTiles Tools](https://github.com/protomaps/go-pmtiles)
- MBTilesTileDataSource - Similar offline tile format
- PersistentCacheTileDataSource - For caching
