/**
 * Sponsors — tiers, current sponsors and the funding channels.
 *
 * Tier amounts are the project's own pricing: change them here, nowhere else.
 * Add a sponsor by dropping a logo in `static/img/sponsors/` and appending an
 * entry to `Sponsors` with its tier id.
 */

export const SponsorContact = 'contact@akylas.fr';

export const Tiers = [
  {
    id: 'platinum',
    name: 'Platinum',
    amount: '20 000 € / year',
    accent: '#8f9bb3',
    benefits: [
      'Large logo on the homepage and this page',
      'A seat in the roadmap discussion — your priorities go to the top',
      'Direct technical contact with the maintainer',
      'Named in release notes',
    ],
  },
  {
    id: 'gold',
    name: 'Gold',
    amount: '10 000 € / year',
    accent: '#d9a441',
    benefits: [
      'Logo on this page',
      'Roadmap input on the items you fund',
      'Direct technical contact with the maintainer',
    ],
  },
  {
    id: 'silver',
    name: 'Silver',
    amount: '5 000 € / year',
    accent: '#9aa5ad',
    benefits: ['Logo on this page', 'Priority triage on the issues you open'],
  },
  {
    id: 'bronze',
    name: 'Bronze',
    amount: '1 000 € / year',
    accent: '#b07b52',
    benefits: ['Name and link on this page'],
  },
];

/**
 * Funded feature work is quoted per item — it is not a tier. Reach the same
 * address; the deliverable is a merged PR and its documentation.
 */
export const CustomWork = {
  title: 'Fund a specific feature',
  body:
    'Need a platform, a renderer feature or a fix on a schedule? That is quoted per item and ' +
    'lands in the open-source repo like everything else — no private fork, no dual licence.',
};

/** {name, url, logo (path under static/), tier} — empty until the first one signs. */
export const Sponsors = [];

export const Channels = [
  {
    id: 'company',
    icon: '🏢',
    title: 'Companies',
    body:
      'Pick a tier and email us. We invoice, you get a logo, a say in the roadmap and a direct ' +
      'line to the maintainer.',
    cta: {label: `Email ${SponsorContact}`, href: `mailto:${SponsorContact}?subject=Massif%20Maps%20sponsoring`},
    available: true,
  },
  {
    id: 'github',
    icon: '💜',
    title: 'Individuals',
    body:
      'Monthly or one-off through GitHub Sponsors. Any amount helps — it is what pays for the ' +
      'devices the renderer is tested on.',
    cta: {label: 'Sponsor on GitHub', href: 'https://github.com/sponsors/massif-maps'},
    available: true,
  },
  {
    id: 'opencollective',
    icon: '🧾',
    title: 'Open Collective',
    body:
      'For organisations that need a transparent, publicly auditable ledger. Not set up yet — ' +
      'tell us if this is the channel you need and it moves up the list.',
    cta: {label: 'Not available yet', href: null},
    available: false,
  },
];

/** Badges users can put in their own README. */
export const Badges = [
  {
    id: 'built-with',
    label: 'Built with Massif Maps',
    file: '/img/badges/built-with-massif-maps.svg',
  },
  {
    id: 'sponsor',
    label: 'Massif Maps Sponsor',
    file: '/img/badges/massif-maps-sponsor.svg',
  },
];
