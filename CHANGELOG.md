# Changelog

All notable changes to this project will be documented in this file. See [standard-version](https://github.com/conventional-changelog/standard-version) for commit guidelines.

## [v6.0.0] - 2026-08-16
### BREAKING CHANGES
- due to [`af6557d`](https://github.com/massif-maps/MassifMaps/commit/af6557df7bd9d625f50b9bcd0fdcbe060fb79229) - stop draping vt lines, and trace contour tiles in one pass *(PR [#47](https://github.com/massif-maps/MassifMaps/pull/47) by [@farfromrefug](https://github.com/farfromrefug))*:

  vt tile lines over 3D terrain are drawn as geometry instead of  
  being baked into the drape texture. They are sharp and keep their style width at  
  any slope, but they no longer thin out with distance and they carry a polygon  
  offset against the surface. Call TerrainOptions.setDrapeLinesEnabled(true) to  
  restore the previous rendering.  
  Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>  
  ---------

- due to [`0d3cf89`](https://github.com/massif-maps/MassifMaps/commit/0d3cf89305f99edd0a8b191efde742019c5627c6) - make panning a 3D terrain map affordable again *(PR [#49](https://github.com/massif-maps/MassifMaps/pull/49) by [@farfromrefug](https://github.com/farfromrefug))*:

  TerrainOptions gains FarPlaneFactor, defaulting to 2 rather than to the previous  
  behaviour, so a terrain app's far plane moves unless it sets the factor to 0. Every binding needs  
  its wrappers regenerated (gradle never runs SWIG).  
  Also adds the %ignore entries for TileLayer's internal terrain and drape methods. They were public  
  on the class but never in the ignore list, and since generated/ is gitignored the stale wrappers  
  hid it - a regeneration exposed renderTerrainGround(Color) and broke the Java build.  
  Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>  
  * test(terrain): switch for the per-vertex DEM tap count  
  debug.carto.demtaps 16|4|1. 16 (the default) keeps today's lattice clamp, 1 is what tangram's  
  terrain vertex does. Measured on the Crosscall with SurfaceFlinger timestats, north pan, background  
  + hillshade only: 11.2 / 12.2 / 13.6 fps.  
  Requires libs-carto (vt: make the per-vertex DEM tap count switchable).  
  Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>  
  * perf(terrain): retire the per-tile background meshes under a shared ground  
  Pointer bump plus the debug.carto.tilebg switch that restores them for measurement.  
  Requires libs-carto (vt: drop the per-tile background mesh under a shared ground).  
  Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>  
  * docs(renderers): the head-to-head with tangram, and what it ruled out  
  First session measuring our fps and tangram's on the same device, in the same motion, with the same  
  instrument. Terrain is at parity (16.2 vs 17.9); with roads and labels we are 2.2x slower (8.3 vs  
  18.1) while drawing less, so the whole gap is the layer pass.  
  Most of it is negative results, which is the point - four things we were about to optimise are  
  measured not to matter: geometry volume (3x fewer indices buys 6.5%), per-vertex DEM taps (nothing  
  once content is on), tile LOD granularity (supersedes the +11% in section 7.2, measured with the  
  wrong instrument), and paint-as-ground (nothing, twice - so the hillshade keeps its place in the  
  layer order for free). The GPU finishes in 38-53 ms of a 120-175 ms frame: we are CPU-bound.  
  What the probes found instead: the terrain state block of TileRenderer::onDrawFrame costs 0.10 ms  
  when nothing changes and up to 20.98 ms when a DEM tile lands, because invalidateTileSurfaces  
  rebuilds tile surfaces on the render thread. That is the pan hang Martin reported. Tangram never  
  rebuilds - their surface is one static shared grid and elevation only replaces a texture.  
  Also records tangram's contour labels (contourTextStyle.cpp: a 4x4 seed grid marched along the  
  elevation gradient, no contour geometry or tile source at all) against ours, which still drags in a  
  whole contour tile set for the labels while the lines are already a free fragment block.  
  And the measurement rules the session had to learn the hard way: PROF is not comparable across apps,  
  their 3D chip does not survive a restart, and the device throttles enough that only interleaved arms  
  inside one run mean anything.  
  Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>  
  * docs(renderers): how tangram avoids the render-thread surface rebuild  
  The pan hang (section 11.5) needs their design, not a patch of ours, so record it: RasterStyle::build  
  creates ONE 64x64 grid of 2 x GL_SHORT vertices at scene load and never rebuilds it, so a DEM tile  
  arriving is a texture upload and nothing else - there is no per-tile terrain mesh to invalidate.  
  Everything needing elevation on the CPU reads the texture instead: ElevationManager::elevationLerp  
  over tex.bufferData() with a one-entry memo for labels, markers and the contour label builder, and  
  renderTerrainDepth + getDepth(screenpos) for occlusion and picking.  
  Ours already has the shared grid and the draw path uses it in regular-grid mode  
  (GLTileRenderer.cpp:3528), but setVisibleTiles still builds per-tile CPU surfaces and every  
  elevation change throws them away - geometry the renderer does not draw in that mode. Their only  
  remaining consumers are the raycast path and the non-grid draw path.  
  Names the three steps in their order, and the detail that makes it a stall: resetTileSurfaces is  
  debounced by SURFACE_RESET_DELAY, invalidateTileSurfaces is not.  
  Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>  
  * test(renderers): print the tile surface build and invalidate counters  
  RenderStats collected tileSurfacesBuilt/tileSurfacesInvalidated but printed neither, so  
  "the render thread rebuilds tile surfaces" went unchecked for a session. They read 0 in  
  every terrain configuration we ship.  
  Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>  
  * perf(terrain): build the elevation texture's bitmap on the encode worker, not the render thread  
  Bitmap::loadFromUncompressedBytes copies the padded 514x514 RGBA texture byte by byte;  
  doing it in uploadReadyTextures put that copy, and the free of the encode buffer, on the  
  render thread - 37% of it (simpleperf, Crosscall, north pan). The worker now hands over  
  the Bitmap and keeps one scratch buffer for every job, so the render thread only uploads.  
  Interleaved device A/B, north pan, hillshade + contours, 3 repeats per arm: 7.3 -> 8.7 fps  
  median, frame 118.6 -> 96.4 ms, layers 49.5 -> 26.0 ms, p25 3.9 -> 8.1 fps. Ridge camera  
  screenshot diff 0.71% of pixels, mean 0.36/255 - label placement timing, not rendering.  
  Requires libs-carto fd20973 (vt: sort key once per tile).  
  Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>  
  * docs(renderers): the render thread, profiled - and the dead end it killed  
  Record how to run simpleperf against the render thread (per-tid, symfs mirroring the  
  device path), what it found, and that the per-tile surface rebuild §11.5 blamed for the  
  pan hang does not happen at all in grid mode.  
  Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>  
  * build(demo): let the bench build the native SDK optimized  
  The debug variant sets no CMAKE_BUILD_TYPE for the native side, so clang compiles the  
  whole SDK at -O0 - which is what every perf number taken from this demo has measured.  
  -PnativeOpt builds it RelWithDebInfo instead, keeping -g so simpleperf still symbolizes.  
  Interleaved on the device, same commit, north pan: 8.6 -> 12.8 fps median, frame  
  97.3 -> 41.6 ms, prelude 17.5 -> 1.7 ms, layers 26.2 -> 8.8 ms.  
  Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>  
  * docs(renderers): every number so far was measured on an unoptimized build  
  Record the -O0 finding, what -PnativeOpt changes, and which earlier conclusions it  
  invalidates - the tangram head-to-head compared their release APK to our -O0 one, and a  
  hot list taken at -O0 over-weights whatever the optimizer would have inlined.  
  Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>  
  * build(demo): optimize the native SDK by default, not behind a flag  
  Android Studio passes no gradle properties, so -PnativeOpt left every run from the IDE  
  at -O0 - which is where the perf numbers come from as often as from the command line.  
  RelWithDebInfo is the default now; -PnativeOpt=false restores -O0 for native debugging.  
  Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>  
  * feat(datasources): generate contour LABEL STUBS instead of contour geometry  
  Port of tangram's ContourTextStyleBuilder (core/src/style/contourTextStyle.cpp), their  
  algorithm and their constants: a 4x4 grid of seeds per tile, each walked down the  
  elevation gradient onto round(elev/interval)*interval - at most 12 iterations,  
  interpolating straight onto the level once it is bracketed - then along the contour  
  tangent until the stub is long enough to lay the elevation text along.  
  The point is what the tile then carries: a handful of ~20 point polylines instead of the  
  full traced contour geometry, for a stack that draws the contour LINES in the terrain  
  shader (HillshadeRasterTileLayer.setContourEnabled, free) and only needs geometry for the  
  labels. Emulator counters at the ridge camera: geometry draws 2035 -> 1217, indices  
  51.6M -> 29.4M, render tiles 1197 -> 712.  
  The features keep the layer name and the 'ele'/'div' attributes, so existing '#contour'  
  text rules style them unchanged, and carry 'stub' (1 stub / 0 traced) so a style keeps its  
  line rules with a [stub=0] filter. Both modes set it: an undefined attribute does not  
  compare equal to 0, so a one-sided property would drop the traced lines instead.  
  CompositeVectorTileLayer applies 'contour-label-stubs' / 'contour-label-interval' from the  
  style, like the other generation parameters of a merged contour source.  
  The stub levels must match the interval the shader draws or the labels sit between the  
  lines - the same note tangram carries in their source.  
  Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>  
  * feat(demo): switch the contour source to label stubs, with the lines from the shader  
  --es contourStubs true --es contourStubInterval 100 --es hsContours true, plus the style  
  side of both: the composite hillshade slot reads its contour settings from the STYLE  
  (hillshade-contour-interval), not from the HillshadeRasterTileLayer setters, which only  
  reach a stand-alone layer - so the demo's hsContours flag drew no lines at all in the  
  composite base until the inline style carried it.  
  The inline '#contour' line rules move under [stub=0] so the stubs are not painted as  
  dashes, and the block passes contour-label-stubs / contour-label-interval through.  
  Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>  
  * docs(renderers): contour labels without contour geometry  
  What the port does, the two traps (levels must match the shader's interval; the composite  
  hillshade slot is style-driven), the emulator counters, and that the device A/B is still  
  to take.  
  Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>  
  * docs(renderers): a full technical documentation set for the rendering path  
  docs/rendering/, split by subsystem so a reader - human or AI - can open one page and  
  get a whole area without loading the rest: the frame and its threads, tiles and LOD, the  
  GL draw path, 3D terrain, the depth model, labels, hillshade/contours, lighting/sky/fog,  
  the composite layer, the performance method, and the tangram comparison.  
  Every page states how it is implemented, how tangram does it, and why we differ where we  
  do - with the measurements that decided it. The RTT drape is deliberately not documented:  
  it is being removed, and the docs say so rather than leaving a reader to find out.  
  render-performance.md becomes explicitly the lab notebook (rounds, dead ends, numbers) and  
  points at the design set; CLAUDE.md points there too.  
  Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>  
  * docs(renderers): vector elements, billboards and picking  
  The last uncovered area: the per-kind element renderers, billboard placement and terrain  
  occlusion fading, the depth rules that differ from tile content and why, and the picking  
  path - including the gap that in regular-grid mode the bitmap intersection finds nothing  
  because the per-tile CPU surfaces it walks are never built.  
  Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>  
  * docs(renderers): say what the rendering set does not cover  
  Startup, spherical projection, post-process effects and platform specifics are not  
  documented; a reader should not have to discover that by searching for them.  
  Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>  
  * perf(terrain): patch an elevation texture's border ring instead of re-encoding it whole  
  A neighbour DEM grid landing changes only the 2-texel ring of a tile's padded texture -  
  the border itself, and this grid's outermost row/column where a coarser neighbour  
  box-filters it. Everything else comes from the tile's own grid and cannot change. The ring  
  is now encoded on the worker and patched into the texture that is already on the GPU  
  (glTexSubImage2D) and into the bitmap behind it, which is what the texture is rebuilt from  
  after a context loss.  
  Measured on device over a cold load plus a zoom sequence: full re-encodes 353 -> 24, with  
  118 border patches instead - 93% of the megabyte re-encodes gone.  
  It does NOT move the frame rate, at any camera tried (north pan 14.6 vs 15.0, cold load  
  10.5 vs 10.6, full DEM detail 10.1 vs 10.7 - all inside the noise): the encode already ran  
  on a worker and the upload is already budgeted, so the render thread never saw it. What is  
  saved is worker CPU, memory bandwidth and battery. In steady state the pipeline is idle  
  either way - a warm pan measured ZERO full encodes, which is worth knowing on its own.  
  The single texel sampler is now shared by the full encode and the ring encode, so the two  
  cannot disagree about a border value. Switchable for A/B:  
    adb shell setprop debug.carto.demborderpatch 0  
  Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>  
  * docs(renderers): the contour stubs on device, and what the DEM path actually costs  
  Stubs measured +14% on the Crosscall. The elevation texture pipeline turns out to be idle  
  in steady state (zero encodes over a warm pan), so the ~9% the profile attributes to it is  
  lookup, not encode - and the border patch, which removes 93% of the re-encodes on a cold  
  load, moves no frame rate at all. Full DEM detail is now -25% rather than -63%.  
  Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>  
  * docs(renderers): fold the device results into the terrain and performance pages  
  The border patch and, more importantly, the fact that the elevation texture pipeline is  
  idle in a warm pan - so that nobody optimises an encode that does not run.  
  Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>  
  * fix(terrain): identify elevation grids by their TILE, not by the pointer behind them  
  The elevation grid cache is an LRU: the same DEM tile can be decoded into a new object at  
  any time. The texture cache compared shared_ptr identity, so a re-decode of unchanged data  
  looked like new data and re-encoded (since border patching, re-patched) a texture whose  
  content was identical - work with no visible result, and a plausible source of terrain that  
  keeps re-stitching while nothing changes.  
  Grids are now compared by tile id, which is stable across re-decodes; a data source change  
  clears the cache outright, so nothing depends on the pointer any more.  
  Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>  
  * fix(labels): glyph SDF fringe - libs-carto pointer bump  
  Requires libs-carto ac4ebba (vt: render the glyph SDF over the full range it is encoded  
  in). Fixes the grey smears hanging off letters, reported at 45.161809/5.709411 z16.34.

- due to [`89ed94e`](https://github.com/massif-maps/MassifMaps/commit/89ed94e2d8bd461f6a4e10592344e167c917583c) - objects in the sky, a camera that can look at them, and the demo rebuilt around it *(PR [#55](https://github.com/massif-maps/MassifMaps/pull/55) by [@farfromrefug](https://github.com/farfromrefug))*:

  Options.setFreeRoam(bool) is replaced by  
  Options.setFreeRoamMode(FreeRoamMode). setFreeRoam(true) becomes  
  setFreeRoamMode(FREE_ROAM_MODE_LOOK).  
  Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>  
  * chore(demo): pick the free roam mode, and drive a two-finger gesture from the panel  
  '--es freeRoam off|look|fps' (the old true/false still maps onto off/look), with sliders for the  
  look sensitivity and the move speed.  
  A two-finger gesture cannot be synthesized with adb - input sends one pointer, and the  
  emulator's touch devices are not writable from the shell - so the panel gets a button that feeds  
  MapView.onTouchEvent a real two-pointer MotionEvent sequence, through the same entry point a  
  finger uses. It logs the focus before and after, which is how the move was measured: a forward  
  drag walks 444 m north, a strafe 355 m west, with no cross-axis drift.  
  Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>  
  * fix(ui): stop the near plane clipping near content when the view pitches up  
  In free roam, looking above the horizon made tiles close to the camera disappear, and the higher  
  the view went the more went with them. The near plane is taken from where the sampled rays MEET  
  THE GROUND, and as the view pitches up those hits walk off into the distance - so the near plane  
  follows them out and clips everything in front of the camera. What is near the camera does not  
  move when the view turns, so a pitched view caps near with the rule that does not depend on the  
  view direction at all: tangram's camera height / 50.  
  Also: PanningSpeedMode, because the same tilted view makes a one-finger pan change speed while  
  the finger is down. The exact grab-the-world pan re-derives its scale from wherever the finger  
  is now, so a drag that starts close to the camera and travels up the screen accelerates -  
  measured at z15 tilt 65, the same 1300 px drag moved 2160 m that way against 1186 m at the speed  
  it started with. ANCHORED (the new default) measures the scale where the gesture starts and  
  keeps it; CONSTANT measures at the centre of the screen, so it depends on nothing; MAP is the  
  old exact behaviour. The two new modes pan by the screen delta in the ground frame, so they also  
  work with the view on the sky, where a map pan has no ground under the touch to hold on to.  
  Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>  
  * chore(demo): rebuild the demo UI around a filterable settings sheet  
  The flat list of every knob had grown past the point of being findable. Now they live in  
  collapsible sections - opening one closes the others, so the list of titles is short enough to  
  read at a glance - and a filter box matches the label of every row across all sections and shows  
  the matches expanded: typing "shadow" beats remembering which section it is in.  
  The rest of the screen: no action bar (the map is the screen), edge to edge with the overlay  
  insetting itself from the system bars and from the keyboard, the readout moved off the status  
  bar to a pill at the bottom left - with the position in it, not just zoom and tilt - and the  
  gear button opposite it, hidden while the sheet is open.  
  Also: constellation names drawn IN the sky, as sprites with a bitmap the demo paints, which is  
  what makes them themeable from the app; and '--es panSpeed map|anchored|constant'.  
  Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>  
  ---------

- due to [`0038ee7`](https://github.com/massif-maps/MassifMaps/commit/0038ee7363ce146dfe10c24071c10f82a64e9baf) - keep the map movable with the camera against the ground *(PR [#77](https://github.com/massif-maps/MassifMaps/pull/77) by [@farfromrefug](https://github.com/farfromrefug))*:

  ViewState.setTerrainMinCameraZ is replaced by  
  setTerrainCameraReference(terrainZ, minCameraZ) and %ignore'd - it is renderer  
  plumbing published every frame, not something an app calls.  
  Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>  
  * fix(demo): stop the tilt going under the ground  
  The demo asked for a tilt range starting at -max(30, lookUp) unconditionally, so  
  a drag could take the view to the horizon and past it on a plain map, where the  
  camera ends up grazing or under the terrain. The range is 30..90 now, and only  
  the two modes that are about looking up - free roam and the star sky - open it.  
  applyLookRange is re-applied when the star sky is left, so the map goes back to  
  30 with it.  
  Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>  
  ---------

- due to [`dfddae6`](https://github.com/massif-maps/MassifMaps/commit/dfddae677ea6232345f865afd9c33c63e06ca02a) - drop the code the fork will not use *(PR [#88](https://github.com/massif-maps/MassifMaps/pull/88) by [@farfromrefug](https://github.com/farfromrefug))*:

  TerrainOptions loses PainterOrderDepthEnabled, RegularGridEnabled and  
  ElementTerrainSlack. All three were already pinned to their only working values; drop the  
  calls.  
  Syntax-checked, SWIG wrappers regenerated. No on-device render check ran - the terrain  
  depth path needs one at a tilted camera over relief before this is called proven.  
  Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>  
  * chore: delete the NMLModelLODTree and GDAL/OGR sources  
  Neither has been in a built profile for the life of this fork: the nmlmodellodtree  
  profile is unreferenced by every build script, and _CARTO_GDAL_SUPPORT is defined by no  
  profile at all. The 42 files still compiled - CMake globs all/native, so every ABI paid  
  23 empty translation units per build - and they still had to be read past.  
  StyleSelector goes with them: it lives under the GDAL guard and OGRVectorDataSource is  
  its only consumer.  
  removes NMLModelLODTreeLayer, On/OfflineNMLModelLODTreeDataSource,  
  NMLModelLODTreeEventListener, NMLModelLODTreeClickInfo, GDALRasterTileDataSource,  
  OGRVectorDataSource, OGRVectorDataBase, StyleSelector and StyleSelectorBuilder, plus the  
  nmlmodellodtree build profile. None of them could be built before, so no working app  
  binds them. NMLModel (the vector element) and libs-carto/nml are untouched.  
  SWIG wrappers regenerated.  
  Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>  
  * docs(rendering): move the render-path measurements out of the code comments  
  93 comment blocks of 9 lines or more across all/native and libs-carto, 1201 lines, most of  
  them measurement logs and dead-end records rather than an explanation of the code under  
  them. Now 5 blocks and 50 lines. Each is one or two lines of why; what was only in the  
  comment moved into docs/rendering/, which is where a measurement belongs:  
  - 02-tiles.md: why fills stay subdivided under draping, the screen-centre LOD elevation,  
    and what tangram's shader displacement costs in index throughput.  
  - 04-terrain.md: normalizing the layer union to a quadtree partition, the drape cache  
    budget / seeding / completeness rules, and how a render tile picks its DEM tile.  
  - 08-lighting-sky-fog.md: the sky quad's horizon clip and the haze start angle.  
  Nothing but comment lines is removed - verified by filtering the diff. Every touched  
  translation unit syntax-checks.  
  Requires libs-carto 22d5ade (vt: the same sweep, plus two commented-out loops deleted).  
  Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>  
  * chore: remove twelve accessors nothing calls  
  Declared and defined, referenced by nothing in all/native, all/modules, generated/ or the  
  platform glue: TileRenderer::isDrapeEnabled / isPlanarTerrainMode /  
  isTerrainPaintFullDetailAllowed, TileLayer::calculateInternalTileBounds,  
  BillboardDrawData::getScreenBottomDistance, FrameBuffer::isColor / isDepth / isStencil,  
  Texture::isMipmaps / isRepeat, PMTilesUtils::tileIdToZxy, and  
  HillshadeRasterTileLayer::getMapTileBitmap, which was declared and never even defined.  
  Found with a symbol-frequency sweep; every hit was then checked against its real call  
  sites, which is what kept convertPointsLists, getSamples, getTileData and the rest of the  
  inline header accessors - a one-line inline definition looks the same as a dead  
  declaration plus definition to a naive count.  
  Members and their setters stay where something still writes them: _screenBottomDistance is  
  read by the billboard sort comparator, and rotateQuadrant by zxyToTileId.  
  None of these are public API (no all/modules entry), so no binding changes.  
  Syntax-checked. PMTilesUtils only parses against a brotli stub - the nested brotli  
  submodule is not checked out here - and its single remaining error is the stub's missing  
  enum value, not this change.  
  Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>  
  ---------


### New Features
- [`5d755cf`](https://github.com/massif-maps/MassifMaps/commit/5d755cf65e1fa079fd7ee8ea8c1c6ca8eaf39d04) - shared regular-grid terrain surfaces + TerrainOptions.RegularGridEnabled *(commit by [@farfromrefug](https://github.com/farfromrefug))*
- [`718b2a2`](https://github.com/massif-maps/MassifMaps/commit/718b2a22b7438c1191ec4ecc3e9ddca4c43cda75) - lattice-clamp draped geometry in regular-grid terrain mode *(commit by [@farfromrefug](https://github.com/farfromrefug))*
- [`a196c18`](https://github.com/massif-maps/MassifMaps/commit/a196c1881d6be5f8f1466b58cf1ea4318bd4cbe2) - skip vector geometry subdivision in regular-grid terrain mode *(commit by [@farfromrefug](https://github.com/farfromrefug))*
- [`5c5a99e`](https://github.com/massif-maps/MassifMaps/commit/5c5a99e6ae8c5c550fcf3509c657735ec169edbc) - painter-order terrain depth model + TerrainOptions.PainterOrderDepthEnabled (Phase D.1) *(commit by [@farfromrefug](https://github.com/farfromrefug))*
- [`a95042b`](https://github.com/massif-maps/MassifMaps/commit/a95042b75e36ec8a1b2e65aaa9733c181a73ccc4) - painter-order surface back-push (experiment A) *(commit by [@farfromrefug](https://github.com/farfromrefug))*
- [`64b5701`](https://github.com/massif-maps/MassifMaps/commit/64b570106ac13d8e206ae5ab350ebdd5d188b4fc) - vector elements draw at real depth in painter-order mode (D.2) *(commit by [@farfromrefug](https://github.com/farfromrefug))*
- [`f3f3240`](https://github.com/massif-maps/MassifMaps/commit/f3f324097144f67b349c6080d28d8dcd3165b359) - **datasources**: ContourTileDataSource - on-the-fly contour lines from a DEM *(commit by [@farfromrefug](https://github.com/farfromrefug))*
- [`c85c997`](https://github.com/massif-maps/MassifMaps/commit/c85c997d403ad2c0997bcb34c4a3eeed8b2f98e7) - **layers**: shader contour lines + CustomRasterTileLayer (custom raster shaders) *(commit by [@farfromrefug](https://github.com/farfromrefug))*
- [`3a317a6`](https://github.com/massif-maps/MassifMaps/commit/3a317a6e2987c3cb0d4fe4630219d82dfe89c526) - on-the-fly contours (ContourTileDataSource) + shader contours + CustomRasterTileLayer *(PR [#18](https://github.com/massif-maps/MassifMaps/pull/18) by [@farfromrefug](https://github.com/farfromrefug))*
- [`bc303b0`](https://github.com/massif-maps/MassifMaps/commit/bc303b0168c3f59a629428a2a4cc21eef6dd3905) - **layers**: CompositeVectorTileLayer — mix external sources into a vector-tile style *(PR [#19](https://github.com/massif-maps/MassifMaps/pull/19) by [@farfromrefug](https://github.com/farfromrefug))*
- [`d5a3427`](https://github.com/massif-maps/MassifMaps/commit/d5a34270fe66d2e2488275849411c9c0e7c635df) - **terrain**: sky, sun lighting, cascaded shadows and cross-layer draping *(PR [#27](https://github.com/massif-maps/MassifMaps/pull/27) by [@farfromrefug](https://github.com/farfromrefug))*
- [`599ec81`](https://github.com/massif-maps/MassifMaps/commit/599ec81abd9ae4acf8e989231cdecba02331c8c4) - DirAssetPackage + AndroidAssetPackage + unified android-dev demo app *(PR [#42](https://github.com/massif-maps/MassifMaps/pull/42) by [@farfromrefug](https://github.com/farfromrefug))*
- [`d6e85e5`](https://github.com/massif-maps/MassifMaps/commit/d6e85e5d58400bc17ebb3f6757557d04372069a2) - **vectortiles**: load fonts from the system when the style does not package them *(PR [#45](https://github.com/massif-maps/MassifMaps/pull/45) by [@farfromrefug](https://github.com/farfromrefug))*
- [`89ed94e`](https://github.com/massif-maps/MassifMaps/commit/89ed94e2d8bd461f6a4e10592344e167c917583c) - **ui**: objects in the sky, a camera that can look at them, and the demo rebuilt around it *(PR [#55](https://github.com/massif-maps/MassifMaps/pull/55) by [@farfromrefug](https://github.com/farfromrefug))*
- [`ae12bdb`](https://github.com/massif-maps/MassifMaps/commit/ae12bdb453bcfcfe6f0ac37f3921415802582380) - **labels**: shield names on the free side, font icons, and a POI test style *(PR [#57](https://github.com/massif-maps/MassifMaps/pull/57) by [@farfromrefug](https://github.com/farfromrefug))*
- [`8abb080`](https://github.com/massif-maps/MassifMaps/commit/8abb080ea84041aa2d10ba7357760fb401abc355) - **geometry**: navigation maneuver arrows, cut from a route and drawn as a line *(PR [#61](https://github.com/massif-maps/MassifMaps/pull/61) by [@farfromrefug](https://github.com/farfromrefug))*
- [`51799a0`](https://github.com/massif-maps/MassifMaps/commit/51799a0456f6961cfae24bdf2d7fe2048b292c21) - **ios**: add an iOS demo bench mirroring scripts/android-dev *(PR [#68](https://github.com/massif-maps/MassifMaps/pull/68) by [@farfromrefug](https://github.com/farfromrefug))*
- [`2c83f5e`](https://github.com/massif-maps/MassifMaps/commit/2c83f5ea9d956ac41be0f5034455881eb9e727a8) - **vectortiles**: change a colour-only style parameter without decoding the tiles again *(PR [#73](https://github.com/massif-maps/MassifMaps/pull/73) by [@farfromrefug](https://github.com/farfromrefug))*
- [`62aaaaa`](https://github.com/massif-maps/MassifMaps/commit/62aaaaa036f8af6862d8ae0970be8ae83068293f) - **vectortiles**: answer a selection style parameter with a repaint instead of a decode *(PR [#76](https://github.com/massif-maps/MassifMaps/pull/76) by [@farfromrefug](https://github.com/farfromrefug))*
- [`952d323`](https://github.com/massif-maps/MassifMaps/commit/952d3233b1601ea07c12fc7a5d05b9074a765fa1) - **vectortiles**: read MapLibre Tiles (MLT) *(PR [#90](https://github.com/massif-maps/MassifMaps/pull/90) by [@farfromrefug](https://github.com/farfromrefug))*
- [`f3c38ee`](https://github.com/massif-maps/MassifMaps/commit/f3c38eec765b5964e7ce910c1b842ee46d080941) - **labels**: font name lists for marker popups and vector elements *(PR [#101](https://github.com/massif-maps/MassifMaps/pull/101) by [@farfromrefug](https://github.com/farfromrefug))*
  - *addresses issue [#4](https://github.com/massif-maps/MassifMaps/issues/4) opened by [@mrpham](https://github.com/mrpham)*

### Bug Fixes
- [`961f7f2`](https://github.com/massif-maps/MassifMaps/commit/961f7f27c1b265dc254c2c3e80f84743ce74a3ba) - stabilize vector-tile label placement while panning *(commit by [@farfromrefug](https://github.com/farfromrefug))*
- [`6bca2c8`](https://github.com/massif-maps/MassifMaps/commit/6bca2c889c5649d0fbafa5854f33564715a30cee) - subdivide draped geometry to the grid cell in regular-grid mode *(commit by [@farfromrefug](https://github.com/farfromrefug))*
- [`a846b2d`](https://github.com/massif-maps/MassifMaps/commit/a846b2d270dd0e83139b283509bf4fd6653edbdc) - painter-order content constant forward offset (Phase D.1 device fix) *(commit by [@farfromrefug](https://github.com/farfromrefug))*
- [`0d4fea5`](https://github.com/massif-maps/MassifMaps/commit/0d4fea5b218d32b95b69e2aaca50787b3145dabf) - allow 0-90 tilt *(commit by [@farfromrefug](https://github.com/farfromrefug))*
- [`b406de5`](https://github.com/massif-maps/MassifMaps/commit/b406de5f018b7d28c84d63e94b1cc110345d832d) - **terrain**: always subdivide fills; source-density was unsafe *(commit by [@farfromrefug](https://github.com/farfromrefug))*
- [`0a1375f`](https://github.com/massif-maps/MassifMaps/commit/0a1375f8f4c5d116ec0603ac9be5b92878798148) - **terrain**: make camera clearance a zoom bound instead of a corrective event *(PR [#23](https://github.com/massif-maps/MassifMaps/pull/23) by [@farfromrefug](https://github.com/farfromrefug))*
- [`2c5a145`](https://github.com/massif-maps/MassifMaps/commit/2c5a145b8fe092c85fb7f14463477018541d4467) - **datasources**: do not dereference a failed tile in PersistentCacheTileDataSource *(PR [#24](https://github.com/massif-maps/MassifMaps/pull/24) by [@farfromrefug](https://github.com/farfromrefug))*
- [`abe7d09`](https://github.com/massif-maps/MassifMaps/commit/abe7d09ef5aba86733182a1b541a21411ec2d356) - **network**: report the underlying Java exception on Android HTTP failures *(PR [#26](https://github.com/massif-maps/MassifMaps/pull/26) by [@farfromrefug](https://github.com/farfromrefug))*
- [`a541d7f`](https://github.com/massif-maps/MassifMaps/commit/a541d7fe284c39e988bc22f87ed0b52c200e23c2) - **datasources**: attach tile metadata in MemoryCacheTileDataSource *(PR [#25](https://github.com/massif-maps/MassifMaps/pull/25) by [@farfromrefug](https://github.com/farfromrefug))*
- [`9a8b823`](https://github.com/massif-maps/MassifMaps/commit/9a8b823a1bdc74ad667eb3a9963f1cce40a0e955) - **layers**: match style attachments in composite group filters *(PR [#28](https://github.com/massif-maps/MassifMaps/pull/28) by [@farfromrefug](https://github.com/farfromrefug))*
- [`348491e`](https://github.com/massif-maps/MassifMaps/commit/348491e50d70f60420dd94ff8cbef694a70ea2af) - **terrain**: stop the drape flashing white and rebuilding itself on zoom out *(PR [#30](https://github.com/massif-maps/MassifMaps/pull/30) by [@farfromrefug](https://github.com/farfromrefug))*
- [`245acb3`](https://github.com/massif-maps/MassifMaps/commit/245acb3bc08e59fb9d3353bf13022b7538f32a22) - **terrain**: stop landcover fills disappearing in non-draped terrain mode *(commit by [@farfromrefug](https://github.com/farfromrefug))*
- [`14086c4`](https://github.com/massif-maps/MassifMaps/commit/14086c407b79d57d3fb9394cb71228ed46d09e59) - **terrain**: stop landcover fills disappearing in non-draped terrain mode *(PR [#32](https://github.com/massif-maps/MassifMaps/pull/32) by [@farfromrefug](https://github.com/farfromrefug))*
- [`c802bfa`](https://github.com/massif-maps/MassifMaps/commit/c802bfaba9862a022c610e1a34fe7ad5df186ff2) - **terrain**: stabilize 3D terrain labels and cut the label/tile churn behind them *(PR [#31](https://github.com/massif-maps/MassifMaps/pull/31) by [@farfromrefug](https://github.com/farfromrefug))*
- [`6e53a02`](https://github.com/massif-maps/MassifMaps/commit/6e53a022b5fc2378aa57566925055e6523513652) - **terrain**: floor the shadow sun altitude for the shadow pass *(PR [#29](https://github.com/massif-maps/MassifMaps/pull/29) by [@farfromrefug](https://github.com/farfromrefug))*
- [`ed7a7a4`](https://github.com/massif-maps/MassifMaps/commit/ed7a7a499bf987b50ecfc4eca9aa97f92a85b380) - **terrain**: apply the view distance right away and fog the ground behind it *(PR [#39](https://github.com/massif-maps/MassifMaps/pull/39) by [@farfromrefug](https://github.com/farfromrefug))*
- [`67801a4`](https://github.com/massif-maps/MassifMaps/commit/67801a4cc26569d1502ba2eb85533c5b50a769ba) - **terrain**: stop far tiles rendering flat by re-clamping the elevation level *(PR [#38](https://github.com/massif-maps/MassifMaps/pull/38) by [@farfromrefug](https://github.com/farfromrefug))*
- [`0c4b691`](https://github.com/massif-maps/MassifMaps/commit/0c4b6916d3ded7229594d1656fe0d07ac6c24fed) - **terrain**: drop drape textures baked from a previous layer stack *(PR [#43](https://github.com/massif-maps/MassifMaps/pull/43) by [@farfromrefug](https://github.com/farfromrefug))*
- [`fe8e74f`](https://github.com/massif-maps/MassifMaps/commit/fe8e74f0664bf35fd25d00ac128a6081ff57eef8) - **labels**: place the labels of composite child layers *(PR [#46](https://github.com/massif-maps/MassifMaps/pull/46) by [@farfromrefug](https://github.com/farfromrefug))*
- [`af6557d`](https://github.com/massif-maps/MassifMaps/commit/af6557df7bd9d625f50b9bcd0fdcbe060fb79229) - **terrain**: stop draping vt lines, and trace contour tiles in one pass *(PR [#47](https://github.com/massif-maps/MassifMaps/pull/47) by [@farfromrefug](https://github.com/farfromrefug))*
- [`221ddde`](https://github.com/massif-maps/MassifMaps/commit/221ddde37f9bf7e5814ad5f0818781961f13e0d4) - **terrain**: stop labels blinking against the terrain depth buffer *(PR [#48](https://github.com/massif-maps/MassifMaps/pull/48) by [@farfromrefug](https://github.com/farfromrefug))*
- [`0612174`](https://github.com/massif-maps/MassifMaps/commit/061217428be15d38fe8e0393b63e6fcf24463060) - **vt**: line joins, and contours that stay meaningful zoomed out *(PR [#50](https://github.com/massif-maps/MassifMaps/pull/50) by [@farfromrefug](https://github.com/farfromrefug))*
- [`5d8319d`](https://github.com/massif-maps/MassifMaps/commit/5d8319d7e84da603f390130915accdc0346cb2ad) - **layers**: stop a long view distance paving the horizon in fine tiles *(PR [#59](https://github.com/massif-maps/MassifMaps/pull/59) by [@farfromrefug](https://github.com/farfromrefug))*
- [`28137f9`](https://github.com/massif-maps/MassifMaps/commit/28137f91f51c2c0c781e805677c9870c818b6ba7) - **ui**: stop the pan axes going arbitrary at a vertical tilt *(PR [#60](https://github.com/massif-maps/MassifMaps/pull/60) by [@farfromrefug](https://github.com/farfromrefug))*
- [`48b826d`](https://github.com/massif-maps/MassifMaps/commit/48b826d2ed02c197a10a6ea8fef46977162ccfd0) - **components**: request a redraw after Layers::setAll *(PR [#69](https://github.com/massif-maps/MassifMaps/pull/69) by [@farfromrefug](https://github.com/farfromrefug))*
- [`872449f`](https://github.com/massif-maps/MassifMaps/commit/872449fbbadb63e720983b838c2f35eaa52ad220) - **vt**: cap a terrain line against the unpacked binormal *(PR [#74](https://github.com/massif-maps/MassifMaps/pull/74) by [@farfromrefug](https://github.com/farfromrefug))*
- [`0038ee7`](https://github.com/massif-maps/MassifMaps/commit/0038ee7363ce146dfe10c24071c10f82a64e9baf) - **terrain**: keep the map movable with the camera against the ground *(PR [#77](https://github.com/massif-maps/MassifMaps/pull/77) by [@farfromrefug](https://github.com/farfromrefug))*
- [`7987602`](https://github.com/massif-maps/MassifMaps/commit/798760233630820897e20bf58063340e8be05b98) - **labels**: bump libs-carto for the callout band, leader line and plate box *(PR [#78](https://github.com/massif-maps/MassifMaps/pull/78) by [@farfromrefug](https://github.com/farfromrefug))*
- [`9c6b6b7`](https://github.com/massif-maps/MassifMaps/commit/9c6b6b7fd549c77bbf394503e954827506b67254) - **labels**: query the terrain occlusion depth with the camera it was rendered from *(PR [#87](https://github.com/massif-maps/MassifMaps/pull/87) by [@farfromrefug](https://github.com/farfromrefug))*
- [`f7217e6`](https://github.com/massif-maps/MassifMaps/commit/f7217e69484f21fa23f4088ea819d188008b366f) - **terrain**: keep the shadow caster set a partition of the ground, and cut the shadow cost *(PR [#92](https://github.com/massif-maps/MassifMaps/pull/92) by [@farfromrefug](https://github.com/farfromrefug))*
- [`8bc68ec`](https://github.com/massif-maps/MassifMaps/commit/8bc68ec0a59727c5360bacc1c934ebb28a08d3fc) - **ios**: repair the iOS framework build and the published API reference *(PR [#100](https://github.com/massif-maps/MassifMaps/pull/100) by [@farfromrefug](https://github.com/farfromrefug))*
- [`08c67ee`](https://github.com/massif-maps/MassifMaps/commit/08c67ee027680559e48b8d302c076b411e9af732) - **ci**: repair the Android API reference and the iOS framework build *(PR [#102](https://github.com/massif-maps/MassifMaps/pull/102) by [@farfromrefug](https://github.com/farfromrefug))*
- [`6d27b44`](https://github.com/massif-maps/MassifMaps/commit/6d27b4406ede304af0127b0b93e212742157d75c) - **ios**: unbreak the Mac Catalyst archive *(PR [#104](https://github.com/massif-maps/MassifMaps/pull/104) by [@farfromrefug](https://github.com/farfromrefug))*
- [`dcaa148`](https://github.com/massif-maps/MassifMaps/commit/dcaa1486b5c02c7db5dd7b4c53e371641163c70d) - **ios**: stop prelinking the Mac Catalyst slices *(PR [#106](https://github.com/massif-maps/MassifMaps/pull/106) by [@farfromrefug](https://github.com/farfromrefug))*
- [`37cc2b3`](https://github.com/massif-maps/MassifMaps/commit/37cc2b35198be3c36c05235a39224e2c8ac366c2) - **ios**: pull in date's ios.mm for the timezone symbols *(PR [#107](https://github.com/massif-maps/MassifMaps/pull/107) by [@farfromrefug](https://github.com/farfromrefug))*
- [`31645f0`](https://github.com/massif-maps/MassifMaps/commit/31645f074274c82c51a14132c0c6458227a6563a) - **ci**: fail fast on release-tag drift and restore changelog tag contract *(PR [#111](https://github.com/massif-maps/MassifMaps/pull/111) by [@Copilot](https://github.com/apps/copilot-swe-agent))*
- [`1374252`](https://github.com/massif-maps/MassifMaps/commit/13742520465e3590de3d0e9fb4d0d51ee63cc12f) - create git tag explicitly before draft release to unblock release-preflight *(PR [#112](https://github.com/massif-maps/MassifMaps/pull/112) by [@Copilot](https://github.com/apps/copilot-swe-agent))*
- [`f477146`](https://github.com/massif-maps/MassifMaps/commit/f4771469a926af6e7e545f41e21a742701353f5f) - strip leading `v` from version input and filter malformed tags in release-preflight *(PR [#113](https://github.com/massif-maps/MassifMaps/pull/113) by [@Copilot](https://github.com/apps/copilot-swe-agent))*
- [`556debd`](https://github.com/massif-maps/MassifMaps/commit/556debd47c6b50a3b288ac9bf5a94c7a79b47c31) - **ci**: remove pip cache from setup-python (no requirements.txt) *(PR [#115](https://github.com/massif-maps/MassifMaps/pull/115) by [@Copilot](https://github.com/apps/copilot-swe-agent))*
- [`2da8dd4`](https://github.com/massif-maps/MassifMaps/commit/2da8dd48db54795588a25fef599c8f2b83aaad5a) - **ci**: correct Boost archive checksum in iOS full profile workflow *(PR [#116](https://github.com/massif-maps/MassifMaps/pull/116) by [@Copilot](https://github.com/apps/copilot-swe-agent))*
- [`230a985`](https://github.com/massif-maps/MassifMaps/commit/230a985cd2702c03bb566de4136f66c45a89151f) - cache swig Lib directory alongside binary in build-swig action *(PR [#117](https://github.com/massif-maps/MassifMaps/pull/117) by [@Copilot](https://github.com/apps/copilot-swe-agent))*
- [`3bf99ab`](https://github.com/massif-maps/MassifMaps/commit/3bf99ab7afeafcaf5de953e825d86c68de518a15) - restore SWIG autotools discovery in iOS CI builds *(PR [#118](https://github.com/massif-maps/MassifMaps/pull/118) by [@Copilot](https://github.com/apps/copilot-swe-agent))*


## [Unreleased]
### New Features
- **3D terrain support** (experimental): the map surface can be displaced by a DEM elevation tile source (mapbox/terrarium RGB encoding, the same sources used by `HillshadeRasterTileLayer`).
  - `TerrainOptions` attached via `Options.setTerrainOptions(...)`: elevation data source (+ optional decoder), `exaggeration`, `meshResolution`, `billboardOcclusionEnabled`, `enabled`; `getElevation`/`getElevations` query API sharing the elevation cache.
  - All tile layers (vector tiles incl. 3D buildings, raster overlays, hillshade) are rendered on the terrain, with crack-free LOD transitions between zoom levels. The same data source instance can feed both the terrain and a `HillshadeRasterTileLayer` (wrap it in a `MemoryCacheTileDataSource`).
  - Vector elements (markers, points, lines, polygons, popups, NML models) are placed on the terrain surface; their z coordinate is interpreted as height above terrain.
  - Billboards and vector tile labels hidden behind terrain fade out (with hysteresis to avoid flicker); can be disabled via `TerrainOptions.setBillboardOcclusionEnabled(false)`.
  - Touch handling is terrain-aware: panning keeps the touched terrain point under the finger, pinch/double-tap zoom pivot on the terrain hit, clicks resolve to terrain positions. The camera is not clamped above the terrain surface (v1 limitation): at low altitudes and high tilt the camera can clip into mountains.
  - A terrain depth pre-pass provides a single consistent depth source: draped 2D geometry is depth-tested against it with a configurable clip-space bias (`TerrainOptions.setDepthBias`, default 0.0005), so overlapping draped layers can not z-fight while terrain ridges still occlude everything behind them.
  - Changing the exaggeration or mesh resolution re-tesselates loaded tiles (relatively expensive; debounce UI sliders). Terrain is currently only supported in the `PLANAR` render projection mode.
  - Known v1 limitations: 3D buildings render through a separate overlay buffer and are not occluded by terrain ridges; layers with an explicit `compOp` are composited without terrain depth; small LOD-ring artifacts are possible where neighboring tiles use different elevation tile zooms.
- **Post-process effects** (experimental): `MapRenderer.setPostProcessEffect(...)` renders the map through a custom fullscreen GLSL fragment shader (`PostProcessEffect`), with optional access to a terrain depth pre-pass. Includes a built-in PeakFinder-style relief outline effect (`PostProcessEffect.createReliefOutlineEffect()`).

## [v5.0.0-rc.13] - 2025-10-11
### New Features
- [`332c6c6`](https://github.com/Akylas/mobile-sdk/commit/332c6c6cff5230759092a73ed83bb55cf17032e4) - add fetcDelay to `startDownloadArea` to add delay between each tile request


## [v5.0.0-rc.12] - 2025-06-30
### Bug Fixes
- [`15c9997`](https://github.com/Akylas/mobile-sdk/commit/15c999705ec054801677e2131f67044429724b4b) - nutibillboardline fix *(commit by [@farfromrefug](https://github.com/farfromrefug))*


## [v5.0.0-rc.11] - 2025-06-08
### New Features
- [`f23d26b`](https://github.com/Akylas/mobile-sdk/commit/f23d26b9a9978cd1278bdb4b73e9788de642d51d) - added the ?? operator for cartocss and mapnikvt *(commit by [@farfromrefug](https://github.com/farfromrefug))*


## [v5.0.0-rc.10] - 2025-06-02
### New Features
- [`4ceb7d2`](https://github.com/Akylas/mobile-sdk/commit/4ceb7d279b4e13efae255153d2a572a3c59b33db) - view::rotation and view::tilt *(commit by [@farfromrefug](https://github.com/farfromrefug))*


## [vv5.0.0-rc.9] - 2025-05-25
### Bug Fixes
- [`c8a1f4d`](https://github.com/Akylas/mobile-sdk/commit/c8a1f4d598cc0392ead713c461e3239bb5c2e06e) - valhalla non network penalty fix *(commit by [@farfromrefug](https://github.com/farfromrefug))*


## [v5.0.0-rc.8] - 2025-05-13
### Bug Fixes
- [`64d3035`](https://github.com/Akylas/mobile-sdk/commit/64d3035eb55c28f36ddbe6e498b6d7864a821aca) - with position line shields should not be oriented *(commit by [@farfromrefug](https://github.com/farfromrefug))*


## [v5.0.0-rc.7] - 2025-04-20
### Bug Fixes
- [`9a3256e`](https://github.com/Akylas/mobile-sdk/commit/9a3256e5e8952c69ac60a0b0a922809ca344b793) - exclude_unpaved for bicycle *(commit by [@farfromrefug](https://github.com/farfromrefug))*


## [v5.0.0-rc.6] - 2024-12-06
### Bug Fixes
- [`597cde4`](https://github.com/Akylas/mobile-sdk/commit/597cde4ab792e3caba03178c5f8b27e9a27ba5ee) - valhalla fix


## [v5.0.0-rc.4] - 2024-10-25
### :bug: Bug Fixes
- [`5ec739c`](https://github.com/Akylas/mobile-sdk/commit/5ec739c76556ca5a7da385c0a2f462656ac1c43c) - allow shield to have no shield image
- [`7fef879`](https://github.com/Akylas/mobile-sdk/commit/7fef87973e49eca5874af8be50a939fc8afea67a) - **android**: add support for 16 KB page sizes


## 5.0.0-rc.2 (2024-08-30)


### Features

* add map options to disable rotation gesture without disabling programmatic changes to rotation ([f2ce639](https://github.com/Akylas/mobile-sdk/commit/f2ce639686546d3383e0c8f48b50ba80d3cee9bf))
* add valhalla instruction to RoutingInstruction ([5bd5a9e](https://github.com/Akylas/mobile-sdk/commit/5bd5a9ed672f6361dc1e71fb6e5ceaa0f58e4c4e))
* added const predicate support (with `#variable`) ([f9f182b](https://github.com/Akylas/mobile-sdk/commit/f9f182bbe90e7e46ae70fa6b5923b5fda82a27db))
* added doubleClickMaxDuration option ([51f3861](https://github.com/Akylas/mobile-sdk/commit/51f38617a945b7c5c32fb79dd710e731047803e6))
* added doubleClickMaxDuration option ([533bdf8](https://github.com/Akylas/mobile-sdk/commit/533bdf8865cf30c4415221517f5e61a04bf167c9))
* added new options for `VectorTileSearchService` : `sortByDistance`, `layers`, `preventDuplicates` ([c890104](https://github.com/Akylas/mobile-sdk/commit/c890104385d61fe61ea294df84fcf50924dc00b1))
* added OruxDBDataSource ([748de52](https://github.com/Akylas/mobile-sdk/commit/748de526aa8ddda46dc7af2503ac439b0824b055))
* allow calculateRoute and matchRoute to return rawResult ([a092b66](https://github.com/Akylas/mobile-sdk/commit/a092b663bd0fac1bed757def9379594dc62a2faa))
* basic TextureMapView ([78d3dc7](https://github.com/Akylas/mobile-sdk/commit/78d3dc73230aea770e6ac5c63d25ce0814f0b9ed))
* basic TextureMapView ([85d31dd](https://github.com/Akylas/mobile-sdk/commit/85d31dd90f2805cb451c86aff1d1af001f4d420c))
* custom normalmap shader support ([ba27fc6](https://github.com/Akylas/mobile-sdk/commit/ba27fc6335c8f3bda1c7b94f6f67c227f274d84c))
* exagerateHeightScaleEnabled property ([4269754](https://github.com/Akylas/mobile-sdk/commit/4269754a0164c404dddf4fdc8c484b4a97c5037f))
* GeoJSONVectorDataSource all to add/remove single features ([607350b](https://github.com/Akylas/mobile-sdk/commit/607350bc1ae7920663f1c8ef30a1037f570dc6a2))
* LocalPackageManagerTileDataSource ([2df9f52](https://github.com/Akylas/mobile-sdk/commit/2df9f52d658908ad686aa8702afba957ce007564))
* maxOverZoomLevel for DataSource ([e7d7cd2](https://github.com/Akylas/mobile-sdk/commit/e7d7cd2179ee6ead478ffc58ccb066c2ba32e282))
* maxSourceOverzoomLevel ([47cc12e](https://github.com/Akylas/mobile-sdk/commit/47cc12ea425c5cac99da00a4efbd1ed8a2fd1726))
* MultiOSMOfflineGeocodingService, MultiOSMOfflineReverseGeocodingService ([92e4ece](https://github.com/Akylas/mobile-sdk/commit/92e4ece65d0643b440c366d5e8633adc0f62e57e))
* MultiValhallaOfflineRoutingService ([f6aae4f](https://github.com/Akylas/mobile-sdk/commit/f6aae4f8c609a240cc73c48a0e9de04d3d77e7c8))
* new `LayersLabelsProcessedInReverseOrder` `Options` property ([e82e9fc](https://github.com/Akylas/mobile-sdk/commit/e82e9fc45fcd6c9357179d4c383668a6ac500f70))
* new methods for `MBVectorTileDecoder`:  `setJSONStyleParameters` and `setStyleParameters` ([9ad8e69](https://github.com/Akylas/mobile-sdk/commit/9ad8e6947fee061dbda24495086669fb8d6a8671))
* normal accent color ([44394a3](https://github.com/Akylas/mobile-sdk/commit/44394a3218f84f3def4dacbb3c728c42a4922c0a))


### Bug Fixes

* added missing methods to MultiTileDataSource ([c85e6ca](https://github.com/Akylas/mobile-sdk/commit/c85e6ca934e4c800546a85844146c539722699ec))
* allow shield to have no shield image ([5ec739c](https://github.com/Akylas/mobile-sdk/commit/5ec739c76556ca5a7da385c0a2f462656ac1c43c))
* allow to add a layer to another renderer. The usecase is when an android activity is re created. You might still have the reference to the native layer which you want to add back to the carto map. It is faster to only add it again than to re create all layers ([9ba3241](https://github.com/Akylas/mobile-sdk/commit/9ba324181df07795c923250a7033914d81ab33e4))
* **android:** add support for 16 KB page sizes ([7fef879](https://github.com/Akylas/mobile-sdk/commit/7fef87973e49eca5874af8be50a939fc8afea67a))
* better support for tileMask ([a0c304e](https://github.com/Akylas/mobile-sdk/commit/a0c304e43a03e2b0fd82a0f325dfd716078575ad))
* correctly copy info.plist ([1219ff0](https://github.com/Akylas/mobile-sdk/commit/1219ff03ac18181988a89e137d4e8fd97ed68c89))
* correctly encode/decode GeoJSON properties for `GeoJSONVectorTileDataSource` so that properties with sub-objects are correctly returned in `onVectorTileElementClicked` ([313eb38](https://github.com/Akylas/mobile-sdk/commit/313eb3858541b937149eb7f0f822354526b71aaf))
* correctly handle click events on MultiPoint PointGeometry ([bcd1e83](https://github.com/Akylas/mobile-sdk/commit/bcd1e835729319807185a78a8b8c99d8e5221331))
* correctly handle valhalla route result ([f73a239](https://github.com/Akylas/mobile-sdk/commit/f73a2393c48dd2f5bd7961f649212bece0d015d5))
* correctly query points elevation ([a8a65d8](https://github.com/Akylas/mobile-sdk/commit/a8a65d8fd8278f320068721204ea3bbd4174df49))
* ensure customParameters are always applied ([8bda9ed](https://github.com/Akylas/mobile-sdk/commit/8bda9ed5fbdd5299ec3bf8e8e06b6f382313c456))
* ensure pointIndex is good on multi leg/trip ([659d10d](https://github.com/Akylas/mobile-sdk/commit/659d10ddff3f072e6f1d5f185535aceb0d6fd036))
* fix after merge ([e304cfc](https://github.com/Akylas/mobile-sdk/commit/e304cfc2a2806258d9eda44bc6791a7384cbb2f9))
* fix for api name change ([148eaae](https://github.com/Akylas/mobile-sdk/commit/148eaaedfe240903371c141273e31942c16ec580))
* for now dont crash on wrong geojson feature ([bfc62f9](https://github.com/Akylas/mobile-sdk/commit/bfc62f9d283119f66784d16dcbe589eaad3afeaf))
* fully fixed normalIlluminationMapRotationEnabled ([14ba203](https://github.com/Akylas/mobile-sdk/commit/14ba203201310b0397bb79d16c16279cb2fd9d10))
* hillshade exageration fix on overzoom ([b07c869](https://github.com/Akylas/mobile-sdk/commit/b07c869187dabad03e8478acd1df2e5d8763a0fe))
* hillshade getElevation(s) handle isReplaceWithParent ([d22602f](https://github.com/Akylas/mobile-sdk/commit/d22602f6a96c16d13b395d26a1471bffcdcce2d0))
* hillshader overzoom fix ([19f48bd](https://github.com/Akylas/mobile-sdk/commit/19f48bd474ee41fafe197352e880d22958d3397f))
* if replaced with parent we should return the other one ([c8ea35c](https://github.com/Akylas/mobile-sdk/commit/c8ea35cb62c9bc39591880aa833b204ed22b32fd))
* LocalPackageManagerTileDataSource working ([96b14e9](https://github.com/Akylas/mobile-sdk/commit/96b14e93876be7d3a42812920310fa8b16f815d9))
* missing update for LIGHTING_SHADER_NORMALMAP with accent_color ([1195473](https://github.com/Akylas/mobile-sdk/commit/119547308eee39f34e965b0e2ab204b977919dab))
* MultiDataSource supports maxOverZoomLevel ([36320ad](https://github.com/Akylas/mobile-sdk/commit/36320ad0fd5a655f5e18fb129372c329defd1fc5))
* request parent tile if isReplacedByParent ([5108d4b](https://github.com/Akylas/mobile-sdk/commit/5108d4bd907784cc205c1f28e62f0a85ed8cfbb8))
* searchProxy fix by allowing searchRadius<0 to disable distance check ([65102fc](https://github.com/Akylas/mobile-sdk/commit/65102fcc2d19273aa8e54d2503f671b98cfe146a))
* shader dymanic change fix ([09d144c](https://github.com/Akylas/mobile-sdk/commit/09d144cdcb0d54dd1773dfecbb8ef4fde90c0ae3))
* some JNI cleanup ([6df0354](https://github.com/Akylas/mobile-sdk/commit/6df0354e98bbc5b8163e10e4e1eee853fd475ef9))
* some MultiDataSource improvements ([7e6c8bd](https://github.com/Akylas/mobile-sdk/commit/7e6c8bdbc3090ba21e4541eb3c2dd2a1136302ba))
* support lite mode ([5af204d](https://github.com/Akylas/mobile-sdk/commit/5af204d4d1244ed6f25a94667303aa947be16f26))
* try to fix build on macos ([6357995](https://github.com/Akylas/mobile-sdk/commit/6357995a29bdf95fde03c4238b084618f838e236))
* trying to improve workflow for versioning ([8975155](https://github.com/Akylas/mobile-sdk/commit/8975155ead0604da5e47aa24a4ec903869dc295b))
* ValhallaOnlineRoutingService allow creating without apiKey ([65d8895](https://github.com/Akylas/mobile-sdk/commit/65d8895be14d0b5ef5254ef4c0d9eae1c240ddfa))
* working MultiTileDataSource (renamed from LocalPackageManagerTileDataSource) ([527b8b9](https://github.com/Akylas/mobile-sdk/commit/527b8b95ab4ffdc980e179226545d60534d9429f))

CARTO Mobile SDK 4.4.7RC1
-------------------

### New features:

* Added support for generic expressions in CartoCSS 'Map' element.
* Added support for CartoCSS 'line-miterlimit' property, tweaked join handling in case of offsets/patterns. 
* Generalized CartoCSS font support, added support expression based face names

### Changes, fixes:

* Fixed Angle UWP related threading issues, if multiple views were used.
* Fixed minor synchronization issue with RasterTileLayer
* Improved handling of null blob in TileData
* Improved normal map building for overzoomed tiles, resulting is less artifacts.
* Improved reporting of .so loading errors on Android (re-throw original exception, instead of just logging/failing afterwards)
* Added handling of 'OnPointerExited' event in UWP MapView
* Build script fixes, fixes related tolatest Python versions, Android NDK25 support


CARTO Mobile SDK 4.4.6
-------------------

### Changes, fixes:

* Fixed minor rendering issue with lines joined at steep angles when BEVEL/ROUND join modes were used


CARTO Mobile SDK 4.4.6RC1
-------------------

### New features:

* Added 'getTimeout', 'setTimeout' methods to 'CartoOnlineTileDataSource', 'MapTilerOnlineTileDataSource' and 'HTTPTileDataSource'

### Changes, fixes:

* Fixed iOS specific issue related to SDK not properly handling 'didBecomeActive' notifications, resulting in MapView not being rendered.
* Fixed critical synchronization issue on UWP platform related to stopping rendering loop.
* Fixed flickering issues when MapView was resized on UWP platform.
* Fix global pattern alignment when using 'polygon-pattern' symbolizer.


CARTO Mobile SDK 4.4.5
-------------------

### Changes, fixes:

* Fixed 'PersistentCacheTileDataSource' tile preload canceling not working
* Fixed several cases where tile datasources could be accessed with tile coordinates out of bounds


CARTO Mobile SDK 4.4.5RC1
-------------------

### New features:

* Added 'setFeatureIdOverride' and 'isFeatureIdOverride' methods to 'MBVectorTileDecoder'
* Added 'isAnimationStarted' method to 'MapInteractionInfo'

### Changes, fixes:

* Fixed critical issue with non-ASCII string wrapping on UWP platform
* Fixed missing 'onMapInterAction' callback on double tap zoom
* Changed user initiated zoom behaviour when 'PIVOT_MODE_CENTERPOINT' mode is used, now screen center is used as a pivot point.
* Updated harfbuzz, libwebp and pugixml dependencies to latest stable versions
* Fixed stack overflow issue in external css2xml utility due to missing rules for EXP/LOG functions
* Added 'build id' to Android shared libraries, to help analyze Android native stack traces


CARTO Mobile SDK 4.4.4
-------------------

### New features:

* Feature id is now accessible in CartoCSS using 'mapnik::feature_id' variable

### Changes, fixes:

* Fixed issues with 'feature id' handling in vector tile renderer when feature was used in multiple layers
* Updated harfbuzz dependency to the latest stable version
* Fixed wrong compilation profile used for UWP builds, resulting in missing a few features
* Dropped 'PersistentCacheTileDataSource' from 'lite' compilation profile, making 'lite' SDK build smaller
* Minor tweaks to built-in styles, related to admin boundaries
* Minor fixes related to non-standard SDK profiles
* Minor optimizations


CARTO Mobile SDK 4.4.4RC1
-------------------

### New features:

* Added 'getDefaultLayerBuffer', 'setDefaultLayerBuffer' methods to 'GeoJSONVectorTileDataSource'. This allows controlling buffer size (in tile pixels) for vector tile layers.

### Changes, fixes:

* Restored support for arbitrary expressions in transform arguments (available in 4.3.x but removed from 4.4.0-4.4.3)
* Improved batching for transformed geometries, all non-translated geometries can be now added into a single batch.
* Fixed shield symbolizer issues where background was affected by fill color.
* Fixed several clipping related issues in 'GeoJSONVectorTileDataSource'
* Improved EAGLContext handling for iOS, workaround for a crash when a view is moved out of a window and then back


CARTO Mobile SDK 4.4.3
-------------------

### New features:

* Added an experimental option to configure various 'VectorTileLayer' parameters via project.json nutiparameters
* Added support for configuring vector tile map parameters via project.json
* Updated boost dependency to the latest stable version

### Changes, fixes:

* Build script cleanup


CARTO Mobile SDK 4.4.3RC3
-------------------

### New features:

* Added 'getRendererLayerFilter', 'setRendererLayerFilter', 'getClickHandlerLayerFilter', 'setClickHandlerLayerFilter' methods to 'VectorTileLayer'. These methods allow ignoring certain layers for rendering or click detection.
* Added 'reverse' function support to CartoCSS 'text-transform'

### Changes, fixes:

* Dropped 'doclava' based javadoc generation, documentation for Android is now based on standard JDK doclet
* Improved Android documentation by hiding unneeded wrapping related details
* Fixed regression in 4.4.3RC2 related to parallel requests to 'ValhallaOfflineRoutingService'
* Added better support for 'none' keyword in CartoCSS
* Minor improvements to error reporting for CartoCSS issues
* Fixes and cleanups in Android build script
* Updated internal FreeType library to latest stable version
* Minor speed and size optimizations


CARTO Mobile SDK 4.4.3RC2
-------------------

### Changes, fixes:

* Fixed 'TileLayer' not properly recalculating tiles when visibility changes, causing layer to remain hidden.
* Fixed deadlock in 'ClusteredVectorLayer' when its data source is non-empty with all elements being hidden
* Fixed stale tiles remaining in caches when offline packages were removed
* Fixed subtle synchronization issues in 'PackageManager'
* Added support for parallel requests to 'ValhallaOfflineRoutingService'
* Added javadoc to published Android artifacts to Maven central
* Minor fixes to iOS build script
* Updated internal libjpeg-turbo, harfbuzz libraries to latest stable versions


CARTO Mobile SDK 4.4.3RC1
-------------------

### Changes, fixes:

* Fixed critical coordinate scaling issue in iOS Metal build (occurs only with iPhone 6 Plus, iPhone 7 Plus and iPhone 8 Plus devices)
* Fixed regression in 'GeoJSONVectorTileDataSource' which caused parsing failure with features with non-object properties
* Optimized parsing of complex CartoCSS styles, improving performance by 20-40% for complex styles
* Optimized loading of compiled 'Mapnik' styles by using symbolizer cache, improving performance by up to 50% for complex styles
* Updated internal Valhalla, sqlite, harfbuzz, botan and protobuf libraries to latest stable versions


CARTO Mobile SDK 4.4.2
-------------------

### Changes, fixes:

* Fixed style fallback version in 'CartoPackageManager' (when using 'startStyleDownload' method)
* Changed exception type when encoutering unsupported geometry in 'GeoJSONVectorTileDataSource'
* Minor iOS build script fixes


CARTO Mobile SDK 4.4.2RC1
-------------------

### New features:

* Added 'setSimplifyTolerance', 'getSimplifyTolerance' methods to 'GeoJSONVectorTileDataSource'
* Added support for complex CartoCSS selectors ('when' selectors)
* Added support for 'bevel', 'none' linejoin modes and 'square' linecap mode in CartoCSS.
* Added 'marker-color' property to CartoCSS that can be applied to both file-based markers and built-in markers.

### Changes, fixes:

* Started using API 31 as compilation target on Android
* Implemented better error reporting of undefined variables in CartoCSS translator
* Fixed deadlock in NMLModel.setRotation(axis, angle) method caused by improper synchronization
* Reimplemented 'setLayerFeatureCollection' method in 'GeoJSONVectorTileDataSource' to make it faster by skipping serialization/parsing steps.
* Implemented switching to 'bevel' linejoin at sharp angles when using 'miter' linejoin
* Fixed multiple issues with string escaping in parsers and generators in CartoCSS and MapnikVT library.
* Fixed minor issues related to internal expression -> predicate conversion in MapnikVT library.
* Fixed dash array generation for subpixel wide lines when rendering vector tiles
* Revised feature id generation logic in 'GeoJSONVectorTileDataSource', SDK now uses feature id, if available or a deterministic auto id generation when not available.
* Reduced default simplication tolerance for 'GeoJSONVectorTileDataSource', new default value should not generate visible simplification artifacts
* Converted CartoCSS 'marker-opacity' property to a view-level parameter, so it can be dependent on 'view::zoom'.
* Updated protobuf and harfbuzz libraries to the latest versions
* Disabled Sqlite locking extensions on iOS and MacCatalyst builds
* Minor optimizations


CARTO Mobile SDK 4.4.1
-------------------

### Changes, fixes:

* Set minimum target to iOS 10 for i386 simulator target (due to thread_local not supported on iOS 9)
* Added libc++, libz dependencies to modulemap of iOS framework
* Updated build scripts to support building Swift Packages of the SDK
* Fixed SDK/MetalANGLE linking issue with iOS Metal build causing uncaught exceptions due to networking problems


CARTO Mobile SDK 4.4.1RC2
-------------------

### Changes, fixes:

* Fixed excessive initialization times when MBTilesTileDataSource was used with databases not containing zoom level metainfo
* Fixed potential memory leaks on iOS when network requests fail
* Added 'setDoubleClickMaxDuration' and 'getDoubleClickMaxDuration' methods to Options class
* Added 'extends' support to JSON project files, to reduce copy-paste declarations in map project files
* Added support for CartoCSS 'line-offset', 'line-pattern-offset' attributes
* Added support for CartoCSS 'text-wrap-character' and 'shield-wrap-character' attributes
* Added the following color manipulation functions to CartoCSS: 'hsl', 'hsla', 'red', 'green', 'blue', 'alpha', 'hue', 'saturation', 'lightness'
* Fixed handling of 'text-min-distance' and 'shield-min-distance' CartoCSS parameters
* Improved label id generation for repeated labels, creating more stable label placements
* Minor tweaks to built-in styles
* Minor optimizations to iOS Metal build
* Updated libjpeg, libwebp, freetype, harfbuzz, miniz to latest stable versions
* Minor optimizations


CARTO Mobile SDK 4.4.1RC1
-------------------

### New features:

* Metal build of iOS framework now supports Mac Catalyst apps
* Added ClickInfo class, to store click related information (click type, duration)
* New mode for reducing click event latency when double click handling is not required


### Changes, fixes:

* Re-implemented 'click type detection disabled' mode, click events are now triggered when finger is lifted
* Added setDoubleClickDetection, isDoubleClickDetection methods to Options class to allow reducing click handling latency
* Added setLongClickDuration, getLongDuration methods to Options class to allow configuring long click detection duration
* Classes like MapEventListener, VectorElementClickInfo now contain ClickInfo instance for additional click attributes
* Added support for decoding proprietary Apple 'PNG' files
* Fixed decoding of specific bitmap formats when using CreateBitmapFromUIImage on iOS
* Fixed Android bitmap decoding when non-standard stride sizes are used
* Fixed tile layer refreshing issue when data source bounds changed
* Fixed old view state being used when adding labels to the vector layer
* Updated built-in style asset, tweaked displaying of multilingual names
* Updated MetalANGLE library to the latest stable version, tweaked build settings to produce smaller binaries
* Updated font rendering libraries, tesselation library to the latest stable version
* Various minor optimizations


CARTO Mobile SDK 4.4.0
-------------------

### Changes, fixes:

* Fixed CartoCSS string-expression evaluation issue, causing some misoptimizations
* GeoJSONGeometryReader and GeoJSONGeometryWriter are now RFC7946 compliant and accept null geometry in features.
* GeoJSONVectorTileDataSource now supports features with null geometry and non-object properties
* Added support for shorthand-encoding of 'nutiparameters' in project.json files
* SDK now catches feature processing exceptions earlier and report thems without causing whole tile decoding to fail.
* Fixes to iOS build scripts


CARTO Mobile SDK 4.4.0RC4
-------------------

### Changes, fixes:

* Fixed iOS Cocoapod packaging issues, causing issues with MetalANGLE framework when used within other frameworks
* Introduced 'carto.utils.DontObfuscate' annotation for Android Java library. This can be used to finetune Proguard obfuscation rules.
* Optimized protobuf library compilation, making SDK binaries 3-5% smaller.
* Replaced Cryptopp library dependency in SDK with Botan library, fixing portability issues
* Enabled 'tile blending speed' attribute for HillShaderRasterTileLayer (default value is 0). This also fixes blending artifacts when using the layer.
* Various fixes and tweaks in SDK build scripts


CARTO Mobile SDK 4.4.0RC3
-------------------

### Changes, fixes:

* Fixed issues iOS with simulator targets not working due to problems with latest cryptopp library
* Fixed issues with some 32-bit Android targets due to problems with latest cryptopp library
* Fixed potential deadlock issue with TouchHandler class. Removed redundant 'onMapMoved' callbacks.
* Fixed potential deadlocks in AnimationHandler and KineticEventHandler when certain SDK APIs were used in MapEventListener callbacks
* Changed compilation flags for 32-bit Android targets to make then compatible with really old devices not supporting NEON extensions
* Tweaked compilation flags for Android, binary sizes are now about 10% smaller while critical code paths are better optimized
* Enabled Link Time Code Generation for UWP builds. This results in smaller and faster binaries.
* Various fixes and tweaks in SDK build scripts


CARTO Mobile SDK 4.4.0RC2
-------------------

### New features:

* Implemented smarter caching logic for CARTO online tile sources. New implementation can keep larger number of tiles in memory and uses better zoom-based tile prioritization during eviction.
* Added getLayerBlendSpeed, setLayerBlendSpeed, getLabelBlendSpeed, setLabelBlendSpeed methods to VectorTileLayer, for controlling transition animations.
* Added getTileBlendSpeed, setTileBlendSpeed methods to RasterTileLayer, for controlling transition animations.

### Changes, fixes:

* Fixed critical regression in GeoJSONVectorTileDataSource causing 'unknown pbf type' errors
* Fixed rendering artifacts with larger halo radiuses in vector tile renderer
* Fixed regression with tile loading canceling, causing updates to vector tiles being slow
* Fixed potential synchronization issues regarding tile invalidation and caching
* Fixed layers not being correctly refreshed in rare cases
* Implemented more robust time interval calculation for transition animations
* Various fixes in build scripts


CARTO Mobile SDK 4.4.0RC1
-------------------

### New requirements:

* Android 3.0 (API 11), previously 2.3 (API 9)
* iOS 9.0, previously 7.0
* CocoaPods 1.10.1, previously 1.6

### Key highlights:

* Much faster CartoCSS processing and compilation. Loading and initialization of CARTO vector layers is now about 3x faster. 
* 30-40% faster vector tile decoding performance and 10% lower memory consumption during decoding.
* Reworked tile loading and prefetching algorithms to provide more responsive UX.
* 3D NML models can now be used together with bitmap markers, with same basic features (auto orientation, transition animations, overlap analysis)
* Built-in Valhalla 3.1 routing engine vs Valhalla 3.0 in SDK 4.3.x.
* New 'TextureMapView' class for Android for applications that need to use 'MapView' with fragments.
* Additional map callback that provides detailed information about the user interactions.
* SDK for iOS is now distributed as XCFramework. Previous SDK versions used Universal Frameworks with 'fat binaries'.
* There are now two prebuilt versions of iOS frameworks: a legacy version using OpenGLES rendering backend and a new version using OpenGLES -> Metal API converter that does not use deprecated iOS APIs.
* SDK built-in vector styles now include fonts and glyphs for Arabic, Hebrew, Georgian and Armenian locales.

### New features:

* Added TextureMapView class and MapViewInterface interface to the SDK. TextureMapView is a subclass of android.view.TextureView and behaves better in apps built from fragments. MapViewInterface provides a common interface for both MapView and TextureMapView.
* Added getDescription method to RoutingInstruction. This provides textual description of the instruction. The description depends on the routing instruction, it can be either generated by the engine or by the SDK.
* Added UI based interaction callback to MapEventListener (onMapInteraction method). The callback receives detailed information about the type of the interaction.
* NMLModel is now a subclass of Billboard. This allows using billboard features like special scaling, orientation modes and transition animations for 3D models.

### Removed features and API changes:

* Removed deprecated compressToPng method from Bitmap (replaced with compressToPNG)
* Removed deprecated NMLModel constructors (replaced with constructors with NMLModelStyle argument)
* Removed getGeometryTagFilters and setGeometryTagFilters methods from RoutingRequest. They are replaced with getPointParameter/setPointParameter methods (with 'geometry_tag_filter' parameter)
* Removed setResolution method from TorqueTileDecoder, changed 'resolution' definition for getResolution method to reflect actual resolution defined in CartoCSS

### Changes and fixes:

* Tile prioritization during tile loading has been reworked to provide quicker feedback, by fetching shared parent tiles when appropriate
* Cancelling of tile loading and decoding is more flexible, puts less pressure on tile caches
* Removed duplicate points in Valhalla routing results, consecutive manuevers can now share the endpoints. This uses the same convention as other routing engines, but may potentially break apps that depend on the old behaviour.
* SolidLayer is now deprecated. If really needed, a custom VectorTileLayer or RasterTileLayer can be used instead.
* CartoOnlineRoutingService is now deprecated, third party online routing services should be used instead
* Address is now depreacted and will be removed in future versions. use GeocodingAddress instead (currently a subclass of Address)
* setRotationAngle, getRotationAngle methods are deprecated in NMLModel, use setRotation, getRotation instead.
* Added setRotationAxis, getRotationAxis methods to NMLModel
* Added setOrientationMode, getOrientationMode, setScalingMode, getScalingMode methods to NMLModelStyleBuilder
* Added getOrientationMode, getScalingMode, getModelAsset methods to NMLModelStyle
* All street names (separated using '/') are now included in Valhalla routing results
* Fixed billboard size animations not working when using BILLBOARD_SCALING_WORLD_SIZE size mode
* Fixed potential native crash when geocoding databases were corrupted
* Fixed potential native crash when map packages were corrupted
* Tweaked memory usage of offline packages, fixed potential issues with read/write access rights
* Made SolidLayer work in globe mode
* Added bitmap argument nullptr check to SolidLayer constructor
* Fixed lighting direction calculation in NMLModelLODTreeRenderer (wrong sign)
* Added getAnimationDuration method to TorqueTileDecoder
* Added Resolution property to TorqueTileDecoder for dotnet APIs.
* Fixed getParent method in MapTile to handle negative tile coordinates
* Fixed NMLModel rotation in globe rendering mode
* Fixed complex offline geocoding queries failing due to memory constraints
* Fixed slow loading of Torque tiles
* Optimized handling of color interpolation expression in vector tile renderer
* Started using latest FreeType and HarfBuzz libraries to render localized names
* Replaced 'msdfgen' Signed Distance Field glyph render with official FreeType SDF glyph renderer.
* Reduced memory reallocation when decoding vector tiles
* Dropped glyph preloading when generating fonts to speeds up map initialization
* Improved error reporting for CartoCSS interpolation expression issues
* Better handling and optimization of 'match' operator when compiling CartoCSS property sets
* Implemented various MBVT decoder optimizations, including decoded geometry cache
* Added extra vector tile label sorting rule, to make visible label selection more deterministic
* Added model color support for NML models. This can be set using setColor method in NMLModelStyleBuilder.
* Added support for generic 'frame-offset' filters for Torque styles. Previously only equal comparison was available.
* Added support for cumulative data aggregation for Torque layers
* Changed vector tile background rendering order, fixed stencil configuration detection when FBOs are used.
* Optimized rendering of VT layers with 'comp-op' defined.
* Fixed potential issues when calculating intersections with 3D polygons.
* Changed internal vector tile rendering order, rendering is done done strictly per-layer, not per-tile. This fixes issues when stencil buffer is not available or switched off (Torque rendering). 
* Fixed orientation angle interaction with line placements in TextSymbolizer
* Tweaks to marker placements on line geometry when using MarkersSymbolizer
* Changed argument types of setCapacity in cache classes from unsigned int to unsigned long on iOS, so that >4GB caches can be used on 64-bit targets.


CARTO Mobile SDK 4.3.5
-------------------

### Changes/fixes:

* Minor documentation fixes and updates


CARTO Mobile SDK 4.3.5RC1
-------------------

### Changes/fixes:

* Fixed handling of 'CANCEL' touch actions in Android. This caused mishandling of following touch events.
* Fixed thread race issue when connecting Java directors, causing issues with classes instantiaton
* Changed iOS framework packaging. Fixed several issues with header files, added support for xcframeworks.
* Fixed performance issue when calculating scaling of 3D polygons


CARTO Mobile SDK 4.3.4
-------------------

### Changes/fixes:

* Fixed out of range memory access issues when packing large VT geometries
* Fixed an issue in VT line clipping implementation causing missing initial vertices in border cases
* Optimizations when converting GeoJSON data to vector tile format (GeoJSONVectorTileDataSource)


CARTO Mobile SDK 4.3.4RC1
-------------------

### Changes/fixes:

* Added support for setting routing parameters to SGREOfflineRoutingService (setRoutingParameter, getRoutingParameter methods)
* Added 'placement-priority' support for vector tile labels, allowing setting priorities for individual labels
* Added onSurfaceChanged event to MapRendererListener. This method is called when map is resized.
* Reduced rendering artifacts of wide dashed lines in vector tile renderer
* Better precision when compressing vector tile coordinates, fixes rare visual artifacts
* Fixed critical Xamarin iOS synchronization redrawing/disposing issues, causing exceptions
* Fixed VectorTileLayer rendering issue related to opacity handling
* Fixed watermark options being ignored after initial rendering
* Fixed non-opaque highlight/shadow color handling in HillshadeRasterTileLayer
* Additional safety checks in Android bitmap conversions


CARTO Mobile SDK 4.3.3
-------------------

### Changes/fixes:

* Fixed regression in label ray-hit detection routine when using globe mode


CARTO Mobile SDK 4.3.3RC2
-------------------

### Changes/fixes:

* Fixed critical content scaling issue on iPhone Plus devices
* Started using API 30 as compilation target on Android


CARTO Mobile SDK 4.3.3RC1
-------------------

### Changes/fixes:

* Fixed PersistentCacheTileDataSource not working with large cache files
* Faster initialization of PeristentCacheTileDataSource with large database files
* Tweaks and fixes to vector tile feature click detection, marker images are now used to detect transparent pixels


CARTO Mobile SDK 4.3.2
-------------------

### Changes/fixes:

* Fixed PersistentCacheTileDataSource not working with large cache files
* Changed PersistentCacheTileDataSource to be more conservative when estimating cache file size


CARTO Mobile SDK 4.3.2RC2
-------------------

### Changes/fixes:

* Fixed multiline RTL text formatting in VT renderer


CARTO Mobile SDK 4.3.2RC1
-------------------

### Changes/fixes:

* Added two new properties to HillshadeRasterTileLayer: shadow color and highlight color
* Minor optimization: avoid tile reloading when listener is disconnected from the layer.
* Slighlty higher background thread priority for tile/data loading tasks
* Added dynamic thread creation to CancelableThreadPool when all workers are busy with lower priority tasks. 
* Fixed transform/orientation being ignored when flipping vector tile labels
* Enabled SIMD optimizations for WebP image library for slight performance boost


CARTO Mobile SDK 4.3.1
-------------------

### Changes/fixes:

* Fixed a crashing issue with ClusteredVectorLayer
* Minor documentation updates


CARTO Mobile SDK 4.3.1RC1
-------------------

### Changes/fixes:

* Added HillshadeRasterTileLayer to the SDK. It can be used to add additional height-based shading to the map.
* Added getTileFilterMode/setTileFilterMode methods to RasterTileLayer. This allows to choose raster tile filtering mode between point, bilinear and bicubic filters.
* Changed lighting vector calculation for globe mode - the lighting vector is now always based on the local tangent frame of focus point
* Deprecated compressToPng method in Bitmap class, use compressToPNG instead
* Fixed issues with HTTPTileDataSource when multiple tile download threads were used on iOS, by making HTTPClient thread safe
* Fixed potential native crash when thread pool is downsized
* Fixed thread race between layers and renderers when GL context was lost
* Fixed compatibility issues with older GPUs not supporting high precision in fragment shaders
* Slightly better error reporting for CartoCSS errors
* Slightly better error reporting for PNG reading errors


CARTO Mobile SDK 4.3.0
-------------------

### Changes/fixes:

* Fixed linking issue with Xamarin iOS build
* Minor optimizations for Android build when using JNI
* Minor logging cleanup
* Documentation changes


CARTO Mobile SDK 4.3.0RC3
-------------------

### Changes/fixes:

* Changed shading of building symbolizers in VT renderer. The lighting is now NOT applied to the top of the building. This matches the behavior of Mapnik.
* Enabled support for rendering buildings with negative height in VT renderer
* Fixed cyclical resource manager referencing, causing memory leaks on Android
* Fixed potential timing related crashes happening when disconnecting layers from MapRenderer
* Fixed a deadlock regression in 4.3.0RC2 when bitmap texture cache was released
* Fixed an issue in layer removal code that could cause removing more layers than intended


CARTO Mobile SDK 4.3.0RC2
-------------------

### Changes/fixes:

* Fixed memory leak when switching render projection
* Thread safety fixes when adding/removing layers to the map
* Fixed memory leak in iOS implementation of HTTPClient
* Throw exception instead of crashing when null ptr is used as an argument for Bitmap constructor
* Fixed very high memory consumption when calling getServerPackages method in PackageManager class
* Optimized internal representation of tilemask, reduced memory usage by 5x
* Fixed RasterTileListener not working (regression in SDK 4.2.x vs 4.1.x)
* Fixed issue with font shaping when '\n' symbol is used in text
* Fixed texture coordinate artifacts when using dashed lines in VT renderer
* Removed unneeded error from the log when translating CartoCSS expressions ('Unsupported text expression type ..')
* Updated the way CartoCSS 'marker-feature-id' and 'text-feature-id' properties treat null/zero values and empty strings: now using these special values forces SDK to generate 'auto id'
* Fixed potential vector tile rendering issues on devices that supported OpenGL Vertex Array extension
* Optimized resource usage when layers are removed from the map, the resources are released sooner than before, resulting in smaller application memory footprint
* Fixed rare display corruption issues when OpenGL surface was lost and layers were being removed from the map
* Fixed styling issues with VectorLayers when bitmaps were shared between different vector element styles
* Implemented proper 'line-cap' support for dashed lines
* Added 'custom parameters' option to GeocodingRequest and ReverseGeocodingRequest classes. Custom parameters can be used to customize specific parameters of geocoding engines.


CARTO Mobile SDK 4.3.0RC1
-------------------

This version is a major update and brings several new features and optimizations. Note that due to the inclusion of Valhalla 3,
then binaries of the SDK are considerably larger on Android compared to SDK 4.2.x.

### Key highlights:

* Valhalla 3 routing support. Valhalla 2 routing was supported in SDK 4.1.x and removed from SDK 4.2.x. This release brings Valhalla back but with new major version and lots of improvements. Note that previous Valhalla 2 offline packages are incompatible with Valhalla 3 and can not be used.
* Support for building the SDK with Metal rendering backend on iOS, instead of OpenGLES. This is currently still experimental, as it generates larger binaries and is a bit slower.

### New features:

* A fully featued matchRoute API for matching points to routing network and extracting routing attributes. 
* Added custom metadata support for Layer class (getMetaData, setMetaData, containsMetaDataKey methods in Layer class)
* Support for rendering basemap Point-of-Interests, API for directly controlling POI/building rendering mode (setPOIRenderMode/getPOIRenderMode methods in CartoVectorTileLayer class)
* API for controlling the render style of basemap buildings (setBuildingRenderMode/getBuildingRenderMode methods in CartoVectorTileLayer class)
* Added 'custom parameters' option to RoutingRequest and RouteMatchingRequest classes. Custom parameters can be used to customize routing schemas of specific routing engines.
* New helper classes FeatureBuilder and VectorTileFeatureBuilder
* Moved matchRoute method to base RoutingService interface
* Moved setProfile/getProfile methods to base RoutingService interface
* Moved setLanguage and setAutocomplete methods to base GeocodingService interface.
* Added setMaxResults and getMaxResults methods to base GeocodingService interface.
* Moved setLanguage method to base ReverseGeocodingService interface.
* Added setClickRadius and getClickRadius methods to VectorTileLayer
* Added setMaxResults and getMaxResults methods to all search services. Note that searches are now capped, thus applications may need to configure the limit appropriately.
* Added 'uppercase', 'lowercase', 'length', 'concat', 'match', 'replace' functions to CartoCSS compiler.
* Added 'regexp_ilike' operator to the search API query language to perform case-insensitive substring matching
* Added support for ARM64 UWP target, removed deprecated ARM UWP target.

### Changes/fixes:

* setGeometryTagFilters, getGeometryTagFilters methods in RoutingRequest are deprecated and will be removed in future versions. Instead use more general setPointParameter/getPointParameter methods with 'geometry_tag_filter' parameter name.
* Labels from different VectorTileLayer instances that have 'allow-overlap' flag set to false no longer overlap each other. This changes previous behavior where each VectorTileLayer did not affect other layers.
* SDK does not throw exception anymore when package manager device keys do not match, this fixes issues with TestFlight on iOS
* Tweaked and optimized offline geocoder, mostly affects autocomplete mode
* Better reporting of online Valhalla routing errors
* Added ferry instruction types (enter/leave ferry) to RoutingAction enum
* Fixed search API issues with tiles and non-closed polygons
* Tweaked rendering of lines with round join types to look smoother, especially when used with thin lines
* Suppressed GLKView deprecation warnings on iOS
* Additional NPE safety in OnlineNMLModelLODTreeDataSource
* Fixed native crash when loading 0-sized image files
* Minor improvements to CartoCSS error reporting.
* Made Mapnik-level string expression parsing recursive, fixes subtle issues with complex expressions
* Better SVG compatibility with RGBA color support


CARTO Mobile SDK 4.2.2
-------------------

### Changes/fixes:

* Fixed iOS specific compilation warning in NTExceptionWrapper.h ("This function declaration is not a prototype")
* Disabled LTO on iOS builds (fixes issue with bitcode generation on iOS platform)


CARTO Mobile SDK 4.2.2RC2
-------------------

### Changes/fixes:

* Fixed vector tile click radius of points if 'allow-overlapping' flag was set to true
* Fixed name wrapping of setWatermarkPadding method in Options class on iOS (was setWatermarkPaddingX, now setWatermarkPadding)
* Clearer error reporting when parsing CartoCSS styles
* Improvements and tweaks to text-on-line rendering in vector tiles


CARTO Mobile SDK 4.2.2RC1
-------------------

### Changes/fixes:

* Additional synchronization for iOS events to prevent potential GL calls while app is paused
* Fixed wrong rendering of vector tile labels using 'point-placement' mode 
* Fixed vector tile label transformation handling
* Speed and memory usage optimizations for vector tile labels
* Minor improvements to CartoCSS error reporting


CARTO Mobile SDK 4.2.1
-------------------

### Changes/fixes:

* Optimized symbol tables in Android .so libraries so SDK is now 5% smaller
* Fixed a potential NPE crash in VT glyph rendering code


CARTO Mobile SDK 4.2.1RC2
-------------------

### Changes/fixes:

* Tweaks to built-in styles to better prioritise rendering of low rank street names
* Better Mapnik compatibility by supporting linestring geometry in PolygonSymbolizer, PolygonPatternSymbolizer and BuildingSymbolizer
* Minor tweaks to line placement clipping against frustum in VT renderer
* Use constant padding around labels, fixes obscure issues with label click area for long texts
* Fixed issue with label click handling - due to label geometry merging wrong geometry was returned in certain cases


CARTO Mobile SDK 4.2.1RC1
-------------------

### Changes/fixes:

* The SDK can now be used without calling registerLicense method of MapView class if CARTO basemap services are not needed. In 4.2.1 and later versions we are showing normal CARTO watermark instead of evaluation watermark in this case.
* Added MapTilerOnlineTileDataSource class that can be used for MapTiler or OpenMapTiles tiles
* Added getGeometryTagFilters/setGeometryTagFilters methods to RoutingRequest; they can be used to filter routing endpoints. This is currently supported only when using SGRE routing engine.
* ValhallaOnlineRoutingService is now included in the standard SDK build. It was available in 4.1.x versions but removed from 4.2.0.
* Added clear methods to VariantArrayBuilder and VariantObjectBuilder classes
* Changed the behavior or MapView screenToMap and mapToScreen methods if called before view size is initialized - the SDK now returns NaNs
* CartoPackageManager constructor now throws an exception if it is instantiated without a valid license
* protected loadConfiguration method in CartoOnlineTileDataSource is no longer exposed
* Fixed MapView background clearing issue with Android Q beta versions


CARTO Mobile SDK 4.2.0
-------------------

### Changes/fixes:

* Added support for 'marker-feature-id', 'text-feature-id' and 'shield-feature-id' CartoCSS properties for uniquely identifying labels
* Fixed regression in 4.2.0RC2 vs RC1 regarding VectorTile hit results ordering
* Fixed render projection switching issues in 4.2.0RC1/RC2
* Fixed kinetic rotation clamping issue in 4.2.0RC1/RC2
* Fixed culling related performance issue in ClusteredVectorLayer
* Guards against null pointer exceptions in ClusteredVectorLayer when interfacing with custom builder
* Better handling of horizontal offsetting in TileRenderer


CARTO Mobile SDK 4.2.0RC2
-------------------

### Changes/fixes:

* Added BalloonPopupButton and related classes so that basic interactivity can be added to BalloonPopups
* Major SGRE optimizations: replaced one-to-one routing engine with many-to-many routing engine, using optimized data structures for routing
* Fixed/improved label ordering in vector tile renderer: prefer bigger labels over smaller ones
* Fixed geometry simplifier attached to LocalVectorDataSource causing a crash
* Fixed multiple issues with billboard sorting and ray casting.
* When calculating actual ray hit with billboard or point, SDK now uses actual bitmap to detect if the clicked pixel is transparent
* Implemented more consistent ordering of vector elements
* Changed billboard rendering to ignore depth testing. Better fit with 3D objects.
* Fixed potential rendering issue with GeometryCollections when switching between planar/spherical rendering mode
* Fixed ray-intersection code with Polygon3D, use the closest intersection point, not the first found point
* Fixed subtle flickering in ClusteredVectorLayer animations
* Minor performance optimization by using platform-optimized zlib
* Fixed getElementClickPos method of PopupClickInfo to return click coordinates as pixel coordinates, not normalized-to-size coordinates
* Fixed issue in SDK4.2.0RC1 that caused map rotation to change when setting focus position in globe view mode
* Fixed GeometryCollectionRenderer to accept both clockwise and counterclockwise oriented polygons
* Documentation fixes


CARTO Mobile SDK 4.2.0RC1
-------------------

This version is a major update and brings lots of new features and optimizations. Some features present in older releases are removed or deprecated in this version.

### Key highlights:

* Globe view support. Maps can be displayed in planar mode (as in previous versions) or in globe view mode.
* EPSG4326 support. WGS84 coordinates can be directly used without needing to convert them to EPSG3857.
* Indoor 3D routing by using GeoJSON input and custom routing profiles. We pulled experimental versions with this into 4.1.x releases, but have since made some changes and stabilized it.
* On-the-fly conversion GeoJSON to vector tiles, so that CartoCSS can be used for styling.
* Faster basemaps with several rendering optimizations.
* Better compatibility with Swift on iOS. SDK does not require bridging header anymore and can be simply 'imported'.
* Faster networking on iOS, by better utilizing OS-provided caching.
* Increased security, all basemap services use HTTPS connection by default.
* Startup time on Android has been significantly reduced. Previously low-end devices required more than a second to load the native SDK component. This loading time is reduced by at least 5 times.
* Basemap style parsing and loading is now faster due to smaller font assets and due to internal optimizations.
* SDK is considerable smaller due to several factors:
  - We have removed offline Valhalla routing support from the SDK. It is still available in the repository and SDK can be built with it.
  - We have removed some font assets from the SDK, so Arabic and few other scripts need external fonts.
  - We use carefully tuned compilation flags that produce smaller native binaries on all platforms.
* All SDK components are now open-source. In previous versions we kept one small component (LicenseManager) private, so custom builds could not connect to online services provided by CARTO. Now this restriction is removed.
* Improvements to build scripts, making compiling the SDK easier and less frustrating experience.


### New features:

* Added EPSG4326 projection. This allows to use longitude/latitude coordinates in the SDK directly, without the need to convert them first.
* New class GeoJSONVectorTileDataSource - provides on-the-fly conversion from GeoJSON layers to vector tiles. This is useful for indoor mapping and allows to use SDKs vector tile renderer with CartoCSS styling.
* New class SGRERoutingService for indoor routing. Additional details can be found in Wiki.
* New class MergedMBVTTileDataSource that merges two MapBox Vector Tile sources into one.
* Added addFallbackFont method to VectorTileDecoder class. This can be used to supply universal fallback font (as binary .TTF asset) for basemaps.
* Added setRenderProjection/getRenderProjection methods to Options class, for switching between planar and globe mode.
* Implemented 3D coordinate support for VectorElements. Previously only billboards handled Z coordinate properly, while using non-zero Z coordinate for polygons or lines produced undefined and usually wrong results.
* Added setZBuffering/isZBuffering methods to VectorLayer. Z buffering may be needed if 3D coordinates are used for lines or polygons.
* Added NMLModelStyle and NMLModelStyleBuilder classes for constructing style instances for NMLModels.
* New HTTP connection class for iOS that works better with device proxy settings and provides better download concurrency.
* Added setSkyColor, getSkyColor to Options class
* Added getMidrange method to MapRange
* Added shrinkToIntersection method to MapBounds
* CartoCSS improvements, 'marker-clip' support, 'north-pole-color', 'south-pole-color' map settings support

### Deprecated features:

* NMLModel constructors with explicit model assets are now deprecated. Use constructors with NMLModelStyle argument instead.


### Removed features:

* Built-in map styles are now smaller and load faster due to fewer built-in fonts. Arabic and few eastern scripts that were displayed in previous versions now require custom font assets. These can be supplied to VectorTileDecoder using addFallbackFont method.
* Removed setSkyBitmap/getSkyBitmap methods from Options class. Sky bitmap usage was poorly documented and relied too much on internal implementation. Use setSkyColor instead of setSkyBitmap.
* simplify method is no longer exposed in GeometrySimplifier class and its subclasses.
* Frustum class is removed from the SDK.
* ViewState class does not expose getCameraPos, getFocusPos, getUpVec, getFrustum methods starting from version 4.2.
* setProjectionMode/getProjectionMode methods are removed ViewState class. Setting projection mode never really worked.
* Removed fromInternalScale method from Projection. This method was never expected to be part of public API and was not useful for applications.
* ValhallaOnlineRoutingService, ValhallaOfflineRoutingService and PackageManagerValhallaRoutingService classes are removed from the public build. SDK used customized version of Valhalla that is not compatible with the latest official Valhalla versions and the library made SDK binaries considerably larger. Valhalla support is still present in the code, it is possible to build a custom version supporting these classes.
* CartoVisBuilder and CartoVisLoader classes are removed from the SDK. These classes provided experimental 'vizjson' support, but were never really complete. 'vizjson' is now deprecated by CARTO.


### Changes:

* All online connections to CARTO services are secure by default. Previously some non-critical services used plaintext connections, causing problems with some newer devices (Android 9) having strict security settings.
* MapView screenToMap now returns NaNs in coordinates if mapping from a given pixel is not possible (tilted map when using sky coordinates, for example)
* EPSG3857 toWgs84 does not return longitude in range -180..180 if the input X coordinate is outside of projection bounds.
* Default panning bounds is now ((-inf, -inf), (inf, inf)) instead of EPSG3857 bounds as in previous versions.
* Sky rendering implementation and default sky color has changed
* Restricted panning mode implementation and behaviour has slightly changed
* All internal fields of wrapped SDK classes on Android are now marked 'transient' and are never serialized. In previous versions trying to serialize/deserialize SDK classes caused native crashes during subsequent GC cycle. The new behviour should result in NPEs and not hard crashes.
* Algorithm for placing text on lines in vector tile renderer is re-implemented and should fix previously distorted placements
* iOS HTTP network stack now uses NSURLSession API for better performance and compatibility. Note that this may cause issues with custom HTTP datasources that do not use secure protocol.
* Much faster handling of [view::zoom] parameter in CartoCSS expressions
* Slightly more compact internal vector tile representation for rendering, gives better tile cache utilization and faster performance


### Fixes:

* setColor, setBitmap, setBitmapScale methods in SolidLayer class properly update the view when called.
* Fixed a memory leak in Java-specific BinaryData constructor taking byte array argument
* Fixed setPreserveEGLContextOnPause not properly invoked in Android MapView class
* Improved compatibility with Android devices with very old GPUs
* Minor search API query language fixes, better support for unicode strings
* Fixed vertex array binding issues with NMLModel rendering
* Fixed minor glyph rendering issues causing glyphs to be slightly blurry under tilted view.
* Minor CartoCSS fixes related to patterned symbolizer support
* Fixed OrderedTileDataSource handling of 'replace with parent' flag


CARTO Mobile SDK 4.1.6
-------------------

This update includes performance and stability improvements,
bug fixes and some minor new features. A new routing engine is introduced 
as an experimental feature.

### New features:

* Added experimental indoor routing support via SGREOfflineRoutingService class.


### Fixes/changes:

* A reworked implementation of HTTP connection worker for iOS that fixes airplane mode switching issues.
* ValhallaOnlineRoutingService now connects to MapBox online service instead of defunct MapZen online service
* Added matchRoute method to ValhallaOnlineRoutingServices
* Added 'wheelchair' routing profile support for Valhalla routing services
* Optimized MBTilesTileDataSource constructor with no explicit minZoom and maxZoom arguments, zoom range is now read first from 'metadata' table. If this fails, full table scan is performed.
* getDataExtent method of MBTilesTileDataSource is now more robust for bad values in 'metadata' table
* GeometryCollectionStyle can now be used when importing FeatureCollection consisting of normal points, lines, polygons to LocalVectorDataSource
* Fixed OrderedTileDataSource getMaxZoom method implementation
* AssetPackage class can now be subclassed from applications
* SDK now handles empty vector tiles as a general case, renders them with background color, not as transparent tiles. 
* Compatibility fix for CartoOnlineVectorTileDataSource by handling 404 code according to server changes (display empty ground tile)
* Added missing header to iOS umbrella header (NTCombinedTileDataSource.h)


CARTO Mobile SDK 4.1.4
-------------------

This update includes performance and stability improvements,
bug fixes and some minor new features.

### New features:

* Exposed TileUtils class with several static methods as part of public API
* SDK now supports custom service URLs as online source ids


### Fixes/changes:

* Fixed Android HTTP connection class to use specified request method (previously always GET)
* Fixed JNI local reference overflows in Android HTTP connection class (with HTTP servers returning very long lists of headers).
* Removed unneeded iOS dependency of libstdc++.6 in Cocoapod, fixes build issues with iOS 12
* Fixed the issue with delayed layer initialization, layers were not automatically rendered
* Fixed several options not correctly reflected in renderer state when changed after the MapView was initialized
* Fixed infinite loop in TileLayer update method when called with inconsistent state (zero view dimensions)
* Fixed value clamping issue with Torque tiles (all floating point numbers were rounded to integers)
* Optimized CartoCSS compiler with 10% reduced map initialization time and faster tile loading time
* Better error reporting of CartoCSS issues
* SDK now uses default background bitmap in case of vector basemap with no background defined
* Bitmap class decoder now supports automatic ungzipping. This is a fix for wrongly configured HTTP servers that send gzipped images even when this is not included in accepted encodings.
* Fixed CartoNamedMapsService ignoring template parameter values when instantiating named maps
* Fixed several grouped marker symbolizers being represented by a single marker
* Fixed threading issue with online license management causing potential API token missing from initial HTTP requests
* Fixed WebP library embedding on iOS targets (Xamarin/native), WebP symbols were previously exported, causing potential linking conflicts
* Made Xamarin.iOS build compatible with 'Linker behaviour = Link All' mode by explictly preserving symbols used through reflection


CARTO Mobile SDK 4.1.3
-------------------

This update includes performance and stability improvements,
bug fixes and some minor new features.

### New features:

* Added support for TomTom online geocoding services (TomTomOnlineGeocodingService and TomTomOnlineReverseGeocodingService)
* Implemented multilanguage support for offline geocoding classes (getLanguage, setLanguage methods in OSMOfflineGeocodingService and PackageManagerGeocodingService classes)
* Implemented localization support for Pelias geocoding results (getLanguage, setLanguage methods in PeliasOnlineGeocodingService)
* Implemented proper location bias for all geocoding services, 'location radius' is no longer needed for bias to work
* Implemented opacity attribute for layers (setOpacity, getOpacity). Note that when used (opacity < 1.0), then this feature may have significant performance impact.
* Implemented background color and border support for Text vectorelements (TextStyleBuilder class)
* Implemented ‘break lines’ flag for texts (TextStyleBuilder class)
* Added online API key interface to CartoMapsService and CartoSQLService
* Added NTExceptionWrapper class for catching/handling SDK exceptions in Swift


### Fixes/changes:

* Min API level on Android is now 10 for Xamarin
* Performance fix for CARTO Maps API - use cacheable requests when instantiating named and anonymous maps
* Fixed regression in SDK 4.1.x vs 4.0.x - packages with incomplete zoom levels had wrong tilemasks after serialized/deserialized in database
* Fixed bounds calculation for NML models
* Fixed zoom level handling in ‘restricted panning’ mode
* Fixed ‘restricted panning’ mode when tilt is applied
* Fixed tile cache invalidation issue when all packages are removed from PackageManager
* BalloonPopupStyleMargins class getters were not wrapped as properties for dotnet platforms previously, fixed now
* Optimized label handling in VT renderer for zoom levels > 14
* Optimized 3D buildings and transparent layers in VT renderer on GPUs that use tiled rendering
* Distance based filtering in search API is more robust now (for coordinate wrapping, etc)
* Fixed WKTGeometryWriter to NOT use scientific encoding


CARTO Mobile SDK 4.1.2
-------------------

This is a maintenance release for SDK 4.1.x containing mostly fixes
but also some new features. This version deprecates support
for external MapZen services due to the services being closed.

### New features:

* SDK has support for MapBox online geocoding services.
  New classes MapBoxOnlineGeocodingService and MapBoxOnlineReverseGeocodingService can be used for this.
* All MapZen online service (Pelias and Valhalla) wrappers now include additional methods for specifying custom service URLs.
  This feature was added as MapZen closes all online services as of February 2018.
* Added optional ‘restricted panning’ support to avoid zooming/panning outside world map area. If turned on, then  map area is restricted to maximize visible map. This can be turned on/off using Options.setRestrictedPanning method
* Added custom service URL support for Pelias and Mapbox geocoders and Valhalla routing
* API documentation for iOS is using Jazzy tool, instead of Doxygen. This allows us to show both ObjectiveC and Swift syntax for the API.


### Fixes/changes:

* Implemented fine-grained clipping in VT loader - reduces drawing of invisible geometry and improves performance 
* Removed MapZen-specific handling from CartoOnlineTileDataSource
* Smaller built-in style asset due to optimized fonts
* Proper handling of line-placement of markers and texts with polygon geometry
* Fixed C#-specific API wrapping issue: Polygon3DStyleBuilder and Polygon3DStyle SideColor property was not properly wrapped
* SDK includes latest version of CARTO styles, with minor fixes
* Improved text placement along lines in vector tile renderer
* Fixed text wrapping in vector tile renderer when ‘wrap-before: true’ mode was used
* MapZen-specific code is removed from CartoOnlineVectorLayer
* Minor optimizations in vector tile renderer for faster rendering of transparent features


CARTO Mobile SDK 4.1.1
-------------------

This is a maintenance release for SDK 4.1.x containing mostly fixes
but also some new features.

### New features:

* Implemented route matching support in ValhallaOfflineRoutingService and PackageManagerValhallaRoutingService classes
* Included NMLModelLODTree in the build (missing from all previous 4.x builds)
* Added postcode to geocoding responses
* Implemented building-min-height parameter for CartoCSS
* Improved support for offline Valhalla routing with multimodal profile


### Fixes/changes:

* Improved text placement in vector tile renderer with texts that have non-zero vertical offsets
* Improved tilting gesture handling on UWP
* Performance optimizations for MB vector tile decoder
* Pelias Online geocoding fixes
* Text rendering quality improvements
* Improvement of Mapnik XML styling reader
* Fixed building height issue with built-in basemaps when 3d buildings are enabled
* Fixed vector tile layer elements missing at zoom level 24
* Fixed http:// and https:// handling when accessing CartoCSS external resources
* Fixed subtle background rendering issues on iOS (PowerVR) due to insufficient precision in fragment shaders
* Fixed UWP specific issue - do not try to create EGL context when panel size is 0
* Fixed custom HTTP headers being ignored when using HTTPTileDataSource
* Fixed basemap 3D building height calculation
* Fixed z-fighting/flickering issue with overlapping basemap 3D buildings
* Fixed minor rendering issues with NMLModelLODTreeLayer
* Fixed a small memory leak with vector layers containing NMLModels
* Documentation fixes


CARTO Mobile SDK 4.1.0
-------------------

This is a major release containing many new features, fixes and performance
optimizations.

### Key highlights:

* SDK now supports **geocoding** and **reverse geocoding**. For offline geocoding, custom geocoding packages can be used through PackageManager. We have provided country-based packages (bigger countries like US, Germany have split packages) but custom packages based on bounding box can be also used. For online geocoding, SDK includes wrapper class for MapZen Pelias geocoder; your MapZen API key is required for that.
* SDK has optional support for **MapZen Valhalla routing**. This feature requires a special SDK build as the routing engine is fairly complex and makes compiled SDK binaries approximately 30% larger. Compared to the custom built-in routing Valhalla routing packages are univeral -  single package can be used for car, bicycle or walking profiles. We have prepared country-based packages that can be downloaded  using PackageManager. Also, custom packages based on bounding box are supported. For online Valhalla routing, SDK includes wrapper class that uses MapZen Mobility API.
* New **built-in styles** and vector tile structure. This change is backward-incompatible due to two reasons: the old styles are removed from the SDK and new styles require different tile and offline package sources. New styles are better optimized for lower-end devices and have more consistent information density on all zoom levels. Also, new styles are based on view-dependent zoom parameters instead of tile-based zoom parameters, which gives much more pleasant zooming experience and cleaner visuals at fractional zoom levels.
* SDK supports **offline searching** features from various sources (VectorTileDataSource, FeatureCollection, VectorDataSource) via unified search API. The search API supports search requests based on geometry and distance, metadata and custom SQL-like query language.
* The VectorElements appearing on the map can now have **transitioning animations**. This is currently supported for billboards only (markers, texts, popups). Different animations styles are supported and the effects can be customized.
* SDK 4.1 has major **speed and memory usage improvements** when using ClusteredVectorLayer class. Performance can be up to 10x better compared to SDK 4.0.x and memory usage 2x lower.
* Lots of lower level performance and memory usage optimizations, mostly related to vector tiles.

### API changes:

* The new built-in styles (Voyager, Positron, Darkmatter) use different data schema and are not compatible with *nutiteq.osm* source. Instead, "**carto.streets**"  source must be used. This applies to both online tiles and offline map packages. The old styles (Dark, Grey, Nutibright) and data source continue to work for now, but are no longer included in the SDK and must be downloaded/applied separately. Offline map packages are not updated for nutiteq.osm source.
* The old nutibright, dark and grey styles are no longer included in the SDK and as a result the following CartoBaseMapStyles are removed:  CARTO_BASEMAP_STYLE_DEFAULT, CARTO_BASEMAP_STYLE_GREY, CARTO_BASEMAP_STYLE_DARK. Instead, new styles CARTO_BASEMAP_STYLE_VOYAGER,  CARTO_BASEMAP_STYLE_POSITRON, CARTO_BASEMAP_STYLE_DARKMATTER should be used.
* Public constructors from various vector element Style classes are now hidden, these classes can now be instanced only through corresponding StyleBuilders.
* Removed unsafe clone method from StyleBuilder.
* Removed public constructors for internal 'UI info' classes.
* Removed public constructors for Frustum class
* New CartoStyles package with following changes:
  1) default language is now "en" (before "local")
  2) 'buildings3d' style parameter is no longer used, instead 'buildings' style parameter can be used to control rendering of buildings (0=no buildings, 1=2D buildings, 2=3D buildings)
* Tilemasks used by the offline packages have stricter semantics now and PACKAGE_TILE_STATUS_PARTIAL tile status  is now deprecated (never used by the SDK) and will be removed in the later versions.


### Detailed list of new features:

* New 'geocoding' module that includes following generic classes/interfaces: GeocodingRequest, GeocodingResult, GeocodingService, ReverseGeocodingRequest, ReverseGeocodingService. The module also includes several classes for offline geocoding/reverse geocoding: OSMOfflineGeocodingService, OSMOfflineReverseGeocodingService, PackageManagerGeocodingService, PackageManagerReverseGeocodingService.  For online geocoding the module includes PeliasGeocodingService and PeliasReverseGeocodingService classes.
* The routing module includes three new classes for Valhalla routing: PackageManagerValhallaRoutingService, ValhallaOfflineRoutingService, ValhallaOnlineRoutingService.  These classes are only included in Valhalla-supporting builds.
* New 'search' module for searching features from various sources. The module includes following classes: SearchRequest, FeatureCollectionSearchService, VectorElementSearchService and VectorTileSearchService.  These classes can be used to search features from loaded geojson collections, vector data sources and vector tile data sources.
* Billboards now support fade-in/fade-out animations. AnimationStyle objects can be now attached to billboard   StyleBuilder objects and the specified animations will be used when billboard appear/disappear.
* PackageManager now includes two additional methods: isAreaDownloaded and suggestPackage. These methods can be used to detect is the view area is downloaded for offline use and if not, to get the best package for the area.
* SDK now support optional zoom gestures. Options class includes setZoomGestures/isZoomGestures methods,  when zoom gestures are turned on, SDK automatically interprets double tap as a zoom-in action and two finger tap as a zoom-out action. By default, zoom gestures are not enabled.
* Implemeted RasterTileClickEventListener class for receiving click events on raster tile layers. SDK provides  click coordinates and the raster tile color at the click point.
* Implemented simulateClick method for Layer class. This method can be used to programatically call event handlers of the layer.
* Implemented automatic background/sky color calculation for VectorTileLayers. If background/sky image is not explicitly defined using Options, then appropriate background/sky image is generated by the SDK.  This provides much better experience with dark styles compared vs SDK 4.0.x.
* Implemented setClearColor/getClearColor for Options class to specify background color of the MapView. This can be used to enable partially transparent map views.
* CartoOnlineDataSource has now support for 'water masks' and coarse water tiles are automatically detected and no longer requested from the server, thus reducing latency and providing better user experience.
* Added getDataExtent method TileDataSource class.  SDK uses the datasource extent information when generating tiles and this results in much lower memory usage in some cases (local raster overlays, for example).
* Added getDataExtent method VectorDataSource class.
* Exposed screenToMap and mapToScreen methods of MapRenderer with explicit ViewState argument.
* Added new helper classes VariantArrayBuilder and VariantObjectBuilder for building Variant instances.
* Added containsObjectKey method to Variant
* The performance of the clustering (ClusteredVectorLayer) is improved up to 10x. Also, the memory usage  of the clustering is now 2x lower. Due to the improvements, clusters of 100k points should works well  even on lowend devices.
* Optimized memory usage of LocalVectorDataSource setAll/addAll methods.
* ClusteredVectorLayer now monitors which attributes of elements change and avoids unnecessary costly reclustering.
* Added option to disable clustering animations via setAnimatedClusters method
* Added new option for faster clustering: ClusterElementBuilder includes additional buildClusterElement method (with cluster position and 'count' arguments). ClusterElementBuilder can specify ClusterBuilderMode which determines which of the two buildClusterElement method gets called.
* Lower level vector tile text rendering uses now SDF (Signed Distance Field) glyph representation which gives
  crisper texts especially on high-DPI devices. Also, memory usage of glyph atlas textures is reduced.   Additionally, the rendering artifacts of vector tile texts with large halos and overlapping glyphs are now fixed.
* Better support for shared dictionaries for offline packages to reduce package sizes.
* Added addFeatureCollection method to LocalVectorDataSource
* CartoVectorTileLayer includes static createTileDecoder method that can used to instantiate VectorTileDecoder from built-in styles.
* Added isOpen method to PersistentCacheTileDataSource.
* PersistentCacheTileDataSource now support asynchronous tile download/cache prefill (startDownloadArea method). An optional listener can be used to monitor tile download progress.
* Implemented setVectorTileBufferSize method for CartoMapsService. This method can be used to tweak tile sizes/fix rendering artifacts  when using vector tiles from CARTO Maps API.
* Reduced memory consumption when large vector tiles are used
* iOS: added support for converting 16 bits-per-component UIImages
* UWP: Added mouse wheel support for zooming.
* Optimizations for GeoJSONGeometryReader, loading large geojson files is now approximately 2x faster
* Implemented ClickSize property for MarkerStyle, this allow enlarging of the click area when very small markers are used.
* Faster loading of complex vector tiles, SDK now optimizes CartoCSS styling rules.
* Optimized memory usage of complex Polygon vector elements (up to 25% in complex cases).
* New classes VectorTileFeature and VectorTileFeatureCollection that are used by the new search API
* CartoCSS: Implemented 'pow' operator
* CartoCSS: Added support for metavariables
* Implemented more optimizations in CartoCSS for various degenerate rendering rules: empty text expressions, zero size features, etc
* Added SideColor property to Polygon3DStyle/builder classes. Previously single Color was always used for all faces of the 3D polygon.
* Added toString method to BinaryData
* CartoCSS feature: comp-op support for markers
* CartoCSS: text-size attribute is now evaluated per-frame, allowing to use smooth text size interpolation based on zoom level
* CartoCSS: enabled PointSymbolizer support
* CartoCSS: parser now supports meta-variables


### Fixes:

* Fixed equals/hash implementation for several built-in classes. Previously both methods provided unreliable results.
* Tile layer preloading tweaks - avoid cache trashing and constant refreshing in rare cases, reduce preloading dataset size
* SDK does not show harmless 'failed to decode tile' warning for empty tiles anymore
* Fixed subtle case of duplicate Layer instance handling in Layers container
* SDK allows vector element to be attached to only a single data source, violating this results in an exception now
* Fixed Windows Phone/UWP related pointer handling, previous version assumed MapView control to be at (0, 0) coordinates in the window
* Fixed touch handling issues on Windows Phone when more than 2 fingers are used
* Fixed regression in SDK 4.0.2 vs 4.0.0 when rendering vector tile lines with null width
* CartoCSS: fixed handling of shield-text-opacity and shield-text-transform
* Fixed multigeometry bounds calculations
* Fixed alpha channel handling when translating color interpolation expressions from CartoCSS to rendering library


CARTO Mobile SDK 4.0.2
-------------------

Maintenance release for CARTO Mobile SDK 4.0.x

### Fixes/Changes:

* Enabled stack protector for Android builds for better app security
* Implemented null pointer checks throwing exceptions for various Layers methods, previously such cases could result in native level crashes
* Implemented workaround for Xamarin/Android multithreading issues - native threads were sometimes not automatically registered when managed delegates are called from multiple threads
* Fixed issues with online licenses when license server was unreliable and took long time to respond
* Fixed app token issues with CARTO named map services
* Fixed SDK log filters being ignored/not working
* Fixed CartoCSS marker-transform handling for non-overlapping points
* Fixed VectorTileLayer click detection when custom transform was applied
* Fixed layer background not being properly set when VectorTileDecoder was updated


CARTO Mobile SDK 4.0.1
-------------------

This is a maintenance release for 4.0.x that includes several important reliability and performance fixes, in addition to
some minor new features.

### New features and changes:

* Added Layer visibility control API to CartoVectorTileDecoder (setLayerVisible, isLayerVisible methods)
* Implemented 'screen' and ‘clear’ comp-op support for CartoCSS/vector tile rendering
* Rendering of vector tile layers with multiple line/polygon symbolisers is now optimized as a special case, this is usually done with a single draw call
* Changed moveToFitBounds behaviour - from now SDK does not change zoom level if single point is used for MapBounds
* Better error reporting for CARTO SQL API, including error logging and error parsing
* Minor optimizations in vector tile renderer
* implemented timeout for online license update procedure
* forward-compatible changes for future features in online tile service and offline packages
* Exposed CartoVectorTileDecoder constructor for better integration with CARTO vector overlays
* Added additional CartoOnlineVectorTile constructor with explicit source and built-in style enumeration parameters
* Added countVisibleFeatures method to TorqueTileLayer
* Added comp-op support to points, markers, texts and shields
* Increased internal visible tile cache size by 4x, for really large overlay datasets (does not affect memory usage in normal cases)
* MBTilesDataSource and OfflineNMLModelLODTreeDataSource classes now open database in read-only mode (previously in read-write mode)
* More precise label coverage analysis for transformed labels

### Fixes:

* Fixed Torque tile usage  in MapsService API due to malformed URL
* Fixed deadlock with indirect texts fields in Text and BalloonPopup objects
* Fixed feature batching related issue in vector tile renderer that caused high number of draw calls and low performance
* Fixed 'multiply' comp-op handling with non-opaque alpha values
* Fixed parameter name typo in CartoCSS (instead of 'polygon-pattern-comp-op', 'polygon-pattern-op' was used)
* Fixed performance issue on iOS with empty Text objects
* CartoCSS compatibility fixes for handling negative line widths and marker sizes
* Minor memory usage, speed optimizations
* Added missing NTCartoVectorTileDecoder to iOS umbrella header
* Fixed CartoVectorTileDecoder layer ordering issues
* Fixed regression regarding VisJSON vector sublayer grouping; visibility and attribute info was previously lost
* Fixed handling of zero size ellipse markers in CartoCSS
* Fixed vector tile click detection issues  
* Fixed rare cases on iOS when screen remained black after returning from background state
* Heavily distorted texts are no longer displayed on the map
* Fixed bad_weak_ptr exception when using PersistentCacheTileDataSource
* Fixed crash with some Xamarin Android versions when MapView finalizer is called
* Fixed license registration issues on Windows Phone targets
* Fixed vector tile layers in layergroup ignoring 'visibility' attribute
* Fixed billboard sorting issues causing flickering with overlapping markers/texts/popups
* Implemented clamping for CartoCSS opacity values for better compatibility


CARTO Mobile SDK 4.0.0
-------------------

CARTO Mobile SDK is built on top of [*Nutiteq Maps SDK 3.3*](http://developer.nutiteq.com), and includes over 100 API related improvements, performance updates and fixes. The new API is not compatible with Nutiteq SDK 3.3, but most apps can be converted relatively quickly and most changes are only related to class/module naming. See [Upgrading from Nutiteq SDK](https://github.com/CartoDB/mobile-sdk/wiki/Upgrading-from-Nutiteq) for more details.

Release notes for next releases can be found from [Releases section](https://github.com/CartoDB/mobile-sdk/releases).

### New features and improvements:

* New 'services' module that gives integration with CARTO online services (Maps services, SQL API, high level VisJSON map configuration)
* JSON serializing/deserializing support and JSON based vector element metadata
* Revamped tile layer support, with more shared features between all tile layers including generic UTF grid support for vector/raster tile layers and many other tweaking options
* Vector editing is now available in all builds (Nutiteq SDK included this only in special GIS builds)
* Improved GeoJSON support, supporting GeoJSON features and feature collections
* Improved and more compliant CartoCSS support for vector tiles with 2 times faster CartoCSS parsing/compiling speed
* Additional styling options for vector overlays (lines, 3D polygons)
* Event handling by layer specific listeners
* Full Collada standard material support in NML models
* Usage of exceptions to signal about most common error cases, for example, file access errors, null pointers, out of range indexing
* Faster vector basemap rendering with better text quality
* Faster and higher quality vector overlay rendering (especially lines)
* Click detection and feature introspection for vector tiles

### Removed features:

* Windows Phone 8.1 is no longer supported, as the platform is generally deprecated, only Windows Phone 10 is now supported
* Basic CartoCSS styling support is removed from styles module, full CartoCSS is available for vector tiles
[v5.0.0-rc.4]: https://github.com/Akylas/mobile-sdk/compare/v5.0.0-rc.2...v5.0.0-rc.4
[v5.0.0-rc.6]: https://github.com/Akylas/mobile-sdk/compare/v5.0.0-rc.5...v5.0.0-rc.6
[v5.0.0-rc.7]: https://github.com/Akylas/mobile-sdk/compare/v5.0.0-rc.6...v5.0.0-rc.7
[v5.0.0-rc.8]: https://github.com/Akylas/mobile-sdk/compare/v5.0.0-rc.7...v5.0.0-rc.8
[vv5.0.0-rc.9]: https://github.com/Akylas/mobile-sdk/compare/v5.0.0-rc.8...vv5.0.0-rc.9
[v5.0.0-rc.10]: https://github.com/Akylas/mobile-sdk/compare/vv5.0.0-rc.9...v5.0.0-rc.10
[v5.0.0-rc.11]: https://github.com/Akylas/mobile-sdk/compare/v5.0.0-rc.10...v5.0.0-rc.11
[v5.0.0-rc.12]: https://github.com/Akylas/mobile-sdk/compare/v5.0.0-rc.11...v5.0.0-rc.12
[v5.0.0-rc.13]: https://github.com/Akylas/mobile-sdk/compare/v5.0.0-rc.12...v5.0.0-rc.13
[v6.0.0]: https://github.com/massif-maps/MassifMaps/compare/v5.2.3...v6.0.0
