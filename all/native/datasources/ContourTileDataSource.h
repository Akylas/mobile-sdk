/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_CONTOURTILEDATASOURCE_H_
#define _CARTO_CONTOURTILEDATASOURCE_H_

#include "datasources/TileDataSource.h"
#include "components/DirectorPtr.h"

#include <atomic>
#include <mutex>
#include <string>

namespace carto {
    class ElevationDecoder;

    /**
     * A tile data source that generates vector contour lines on the fly from an
     * RGB-encoded elevation (DEM) tile data source. The wrapped elevation data source
     * is shared (e.g. with a HillshadeRasterTileLayer) so terrain tiles are fetched
     * only once.
     *
     * The generated tiles are standard Mapbox Vector Tiles containing a single line
     * layer (default name "contour"). Each contour feature carries two attributes:
     *   - 'ele': the contour elevation in meters.
     *   - 'div': the largest "nice" divisor of the elevation (1000, 500, 250, 200,
     *            100, 50, 20 or 10), matching the gdal_contour based pipeline. This
     *            lets CartoCSS filter contour importance by elevation, e.g.
     *            #contour[div=500][zoom>=12] { ... }.
     *
     * Attach the source to a normal VectorTileLayer to render lines and labels
     * ([ele] as the label text) fully from the decoder style.
     *
     * Note: this class is experimental and may change or even be removed in future SDK versions.
     */
    class ContourTileDataSource : public TileDataSource {
    public:
        /**
         * Constructs a ContourTileDataSource object.
         * @param dataSource The RGB-encoded elevation data source to generate contours from.
         * @param elevationDecoder The decoder used to convert RGB pixels to elevation. If null,
         *        the decoder is inferred from the data source 'encoding' metadata (defaults to terrarium).
         */
        ContourTileDataSource(const std::shared_ptr<TileDataSource>& dataSource, const std::shared_ptr<ElevationDecoder>& elevationDecoder);
        /**
         * Constructs a ContourTileDataSource object, inferring the elevation decoder from the
         * data source 'encoding' metadata (defaults to terrarium).
         * @param dataSource The RGB-encoded elevation data source to generate contours from.
         */
        explicit ContourTileDataSource(const std::shared_ptr<TileDataSource>& dataSource);
        virtual ~ContourTileDataSource();

        /**
         * Returns the name of the generated vector tile layer.
         * @return The layer name. The default is "contour".
         */
        std::string getLayerName() const;
        /**
         * Sets the name of the generated vector tile layer. This must match the layer id used in the CartoCSS style.
         * @param name The layer name.
         */
        void setLayerName(const std::string& name);

        /**
         * Returns the base contour interval in meters.
         * @return The base contour interval in meters. The default is 10.
         */
        float getBaseInterval() const;
        /**
         * Sets the base contour interval in meters. At low zoom levels a coarser multiple of this
         * interval is generated to save processing (z<=12 uses 10x, z==13 uses 5x, z>=14 uses 1x).
         * @param interval The base contour interval in meters.
         */
        void setBaseInterval(float interval);

        /**
         * Returns the target grid resolution used for contour tracing.
         * @return The target grid resolution. The default is 128.
         */
        int getResolution() const;
        /**
         * Sets the target grid resolution used for contour tracing. The DEM is subsampled so that
         * the traced grid is at most this many samples per side. Lower values produce coarser but
         * much cheaper geometry (fewer vertices to trace, simplify, upload and drape over terrain).
         * @param resolution The target grid resolution (clamped to at least 8).
         */
        void setResolution(int resolution);

        /**
         * Returns the minimum zoom at which contour geometry is generated.
         * @return The minimum contour zoom. The default is 12.
         */
        int getMinVisibleZoom() const;
        /**
         * Sets the minimum zoom at which contour geometry is generated. Below this zoom loadTile returns
         * an empty (but valid) tile without fetching or tracing the DEM. Note: in CartoCSS 'zoom' means the
         * TILE zoom, so the style must also draw the desired zoom range for contours to actually appear.
         * @param zoom The minimum contour zoom.
         */
        void setMinVisibleZoom(int zoom);

        /**
         * Returns whether seamless tile edges are enabled.
         * @return True if seamless edges are enabled. The default is false.
         */
        bool isSeamlessEdgesEnabled() const;
        /**
         * Sets whether to generate seamless tile edges. When enabled, the east/north/north-east neighbour
         * DEM tiles are fetched so that a tile's east and north edges use the exact same elevation samples
         * as the adjacent tiles, removing the small gaps where contour lines meet at tile boundaries.
         * This costs up to three extra DEM tile fetches/decodes per tile (they are usually cached).
         * @param enabled True to enable seamless edges.
         */
        void setSeamlessEdgesEnabled(bool enabled);

        /**
         * Returns the simplification tolerance in tile pixels.
         * @return The simplification tolerance in tile pixels. The default is 1.0.
         */
        float getSimplifyTolerance() const;
        /**
         * Sets the simplification tolerance in tile pixels. Use 0.0 to disable simplification.
         * @param tolerance The simplification tolerance in tile pixels.
         */
        void setSimplifyTolerance(float tolerance);

        virtual int getMinZoom() const;
        virtual int getMaxZoom() const;
        virtual MapBounds getDataExtent() const;
        virtual std::string getEncoding() const;

        virtual std::shared_ptr<TileData> loadTile(const MapTile& tile);

    protected:
        class DataSourceListener : public TileDataSource::OnChangeListener {
        public:
            explicit DataSourceListener(ContourTileDataSource& dataSource);
            virtual void onTilesChanged(bool removeTiles);
        private:
            ContourTileDataSource& _dataSource;
        };

        std::shared_ptr<ElevationDecoder> resolveDecoder(const std::shared_ptr<TileData>& tileData) const;
        double getIntervalForZoom(int zoom) const;
        static long long computeDiv(long long ele);

        const DirectorPtr<TileDataSource> _dataSource;
        const std::shared_ptr<ElevationDecoder> _elevationDecoder;

        std::atomic<float> _baseInterval;
        std::atomic<float> _simplifyTolerance;
        std::atomic<int> _resolution;
        std::atomic<int> _minVisibleZoom;
        std::atomic<bool> _seamlessEdges;
        std::string _layerName;
        mutable std::mutex _mutex;

    private:
        std::shared_ptr<DataSourceListener> _dataSourceListener;
    };

}

#endif
