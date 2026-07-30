/*
 * Minimal CartoCSS style project bundled in the app assets, over the OpenMapTiles schema.
 *
 * WHY IT EXISTS: this is the smallest complete example of a style that a
 * CompositeVectorTileLayer can weave sources into. A slot exists only because
 *   1. project.json "layers" DECLARES the name ('hillshade', 'satellite', 'contour'), which also
 *      fixes the draw order (that array is TOP -> BOTTOM), and
 *   2. this file has a '#name { ... }' rule carrying the per-source settings.
 * A COMPILED Mapnik XML style cannot do this at all - the XML symbolizer set has no hillshade /
 * raster config symbolizer - so composite slots need a CartoCSS project like this one.
 *
 * Text is omitted on purpose: labels would need font assets in the package.
 */

Map {
  background-color: #eef2f0;
}

#water { polygon-fill: #9cc3e0; }
#landuse { polygon-fill: #dddddd; }
#landcover { polygon-fill: #dbe8cc; }

/* Hillshade source, drawn above the land fills and below the roads. Every numeric setting takes
   zoom-dependent expressions; 'nuti::show_relief' makes it a user setting. */
#hillshade['nuti::show_relief'=true][zoom>=4] {
  hillshade-opacity: linear([view::zoom], (4, 0.5), (12, 0.9));
  hillshade-exaggeration: linear([view::zoom], (4, 0.6), (12, 1.2));
  hillshade-illumination-direction: 335;
  hillshade-shadow-color: #473b24;
  hillshade-method: igor;
}

/* Raster source at the same slot position as its first reference. */
#satellite[zoom>=11] {
  raster-opacity: 0.6;
  raster-comp-op: src-over;
}

#transportation { line-color: #ffffff; line-width: 1.2; }
#transportation['class'='motorway'] { line-color: #e27d60; line-width: 3; }

#building[zoom>=14] { building-fill: #d9cfc4; building-height: 14; }

/* Generated contour source: merged into the master source, so it is styled like any vector layer.
   'contour-base-interval' configures the generator itself. */
#contour[zoom>=5] {
  line-color: #9a5a12;
  line-width: 0.8;
  line-opacity: 0.7;
  contour-base-interval: 20;
}
