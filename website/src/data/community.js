export const GitHubOrg = 'https://github.com/massif-maps';

export const Channels = [
  {
    icon: '🐛',
    title: 'Report a bug',
    body:
      'Wrong rendering, a crash, a regression against the original CARTO SDK. Include the ' +
      'platform, the SDK version and a style or tile source that reproduces it.',
    cta: {label: 'Open an issue', href: 'https://github.com/massif-maps/MassifMaps/issues/new'},
  },
  {
    icon: '💡',
    title: 'Ask for a feature',
    body:
      'Feature requests are issues too. The ones that get the `roadmap` label show up on the ' +
      'roadmap page, so this is also how you influence what comes next.',
    cta: {label: 'Request a feature', href: 'https://github.com/massif-maps/MassifMaps/issues/new'},
  },
  {
    icon: '💬',
    title: 'Ask a question',
    body:
      'Not sure whether something is a bug or a usage question? Discussions is the low-stakes ' +
      'place for it — no template, no triage.',
    cta: {label: 'Open a discussion', href: 'https://github.com/massif-maps/MassifMaps/discussions'},
  },
];

export const Repos = [
  {
    name: 'MassifMaps',
    href: 'https://github.com/massif-maps/MassifMaps',
    branch: 'master',
    body: 'The SDK itself — C++ core, SWIG bindings, platform glue, build scripts and this site.',
  },
  {
    name: 'massif-maps-libs',
    href: 'https://github.com/massif-maps/massif-maps-libs',
    branch: 'develop',
    body:
      'Submodule: the GL vector-tile renderer (`vt`), the Mapnik/CartoCSS pipeline, geocoding ' +
      'and the SGRE/OSRM routing engines. Most renderer work lands here.',
  },
  {
    name: 'massif-external-libs',
    href: 'https://github.com/massif-maps/massif-external-libs',
    branch: 'develop',
    body: 'Submodule: vendored third-party dependencies (cglib, freetype, harfbuzz, MLT decoder…).',
  },
];

export const Contributing = [
  {
    title: 'Read BUILDING.md first',
    body:
      'The SDK is a large C++ project — a full build takes an hour and needs a SWIG fork and a ' +
      'boost symlink. The Android demo under `scripts/android-dev` is the fast loop.',
    href: 'https://github.com/massif-maps/MassifMaps/blob/master/BUILDING.md',
    linkLabel: 'BUILDING.md',
  },
  {
    title: 'Conventional Commits',
    body:
      '`feat:`, `fix:`, `chore:` — no commitlint enforces it, the discipline is on us. One PR ' +
      'per repo, submodule PR first, then the pointer bump.',
    href: 'https://www.conventionalcommits.org/',
    linkLabel: 'The convention',
  },
  {
    title: 'Document in the same change',
    body:
      'Anything that changes render behaviour ships with its `docs/rendering/` edit in the same ' +
      'commit — including what was ruled out, not only the fix.',
    href: 'https://github.com/massif-maps/MassifMaps/tree/master/docs/rendering',
    linkLabel: 'docs/rendering',
  },
];
