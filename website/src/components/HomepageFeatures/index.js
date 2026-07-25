import clsx from 'clsx';
import Link from '@docusaurus/Link';

const CoreFeatures = [
  {
    icon: '🗺️',
    title: 'High-performance vector tiles',
    body: 'A flexible OpenGL vector-tile renderer with CartoCSS styling, MBTiles, PMTiles, GeoJSON and Mapbox Vector Tiles support.',
  },
  {
    icon: '📱',
    title: 'Truly cross-platform',
    body: 'One C++ core, native bindings for Android (Java/Kotlin), iOS (Objective-C/Swift) and UWP (C#).',
  },
  {
    icon: '🧭',
    title: 'Routing & geocoding',
    body: 'Embedded Valhalla street routing, SGRE indoor routing, and offline forward/reverse geocoding.',
  },
  {
    icon: '📦',
    title: 'Offline first',
    body: 'Offline map, routing and geocoding packages via the Package Manager — full functionality with no connection.',
  },
];

const NewFeatures = [
  {
    icon: '⛰️',
    title: '3D Terrain',
    to: '/docs/features/3d-terrain',
    body: 'Real 3D elevation with render-to-texture fill draping, correct depth occlusion and fast-zoom performance.',
  },
  {
    icon: '〰️',
    title: 'On-the-fly Contours',
    to: '/docs/features/contours',
    body: 'Generate contour lines directly from RGB elevation tiles — no pre-baked mbtiles — plus GPU shader contours.',
  },
  {
    icon: '🎚️',
    title: 'Composite Vector Layers',
    to: '/docs/features/composite-vector-tile-layer',
    body: 'Weave external raster, hillshade and vector sources into a single CartoCSS style, ordered by layer name.',
  },
  {
    icon: '🌄',
    title: 'Advanced Hillshade',
    to: '/docs/features/hillshade',
    body: 'Multiple hillshade algorithms (GDAL, Igor, multidirectional), exaggeration and custom raster shaders.',
  },
];

function Card({icon, title, body, to, isNew}) {
  const inner = (
    <div className="featureCard">
      <div className="featureCardIcon">{icon}</div>
      <div className="featureCardTitle">
        {title}
        {isNew && <span className="badgeNew">New</span>}
      </div>
      <div className="featureCardBody">{body}</div>
    </div>
  );
  return (
    <div className={clsx('col col--3')} style={{marginBottom: '1.5rem'}}>
      {to ? (
        <Link to={to} style={{textDecoration: 'none', color: 'inherit', display: 'block', height: '100%'}}>
          {inner}
        </Link>
      ) : (
        inner
      )}
    </div>
  );
}

export default function HomepageFeatures() {
  return (
    <>
      <section className="featureSection">
        <div className="container">
          <h2 style={{textAlign: 'center', marginBottom: '2rem'}}>Everything you need to build map apps</h2>
          <div className="row">
            {CoreFeatures.map((props, idx) => (
              <Card key={idx} {...props} />
            ))}
          </div>
        </div>
      </section>

      <section className="featureSection" style={{background: 'var(--ifm-color-emphasis-100)'}}>
        <div className="container">
          <h2 style={{textAlign: 'center', marginBottom: '0.4rem'}}>New in the Akylas fork</h2>
          <p style={{textAlign: 'center', marginBottom: '2rem', color: 'var(--ifm-color-emphasis-700)'}}>
            Features shipping beyond the original CARTO SDK.
          </p>
          <div className="row">
            {NewFeatures.map((props, idx) => (
              <Card key={idx} {...props} isNew />
            ))}
          </div>
        </div>
      </section>
    </>
  );
}
