// @ts-check
import {themes as prismThemes} from 'prism-react-renderer';

/** @type {import('@docusaurus/types').Config} */
const config = {
  title: 'CARTO Mobile SDK',
  tagline: 'Open, multi-platform maps & location services for Android, iOS and UWP',
  favicon: 'img/favicon.svg',

  url: 'https://akylas.github.io',
  baseUrl: '/mobile-sdk/',

  organizationName: 'Akylas',
  projectName: 'mobile-sdk',
  deploymentBranch: 'gh-pages',
  trailingSlash: false,

  onBrokenLinks: 'warn',
  onBrokenMarkdownLinks: 'warn',

  markdown: {
    // Parse .md as CommonMark (safe for vendored guides), .mdx as MDX.
    format: 'detect',
    mermaid: true,
  },
  themes: ['@docusaurus/theme-mermaid'],

  i18n: {
    defaultLocale: 'en',
    locales: ['en'],
  },

  presets: [
    [
      'classic',
      /** @type {import('@docusaurus/preset-classic').Options} */
      ({
        docs: {
          sidebarPath: './sidebars.js',
          routeBasePath: 'docs',
          editUrl: 'https://github.com/Akylas/mobile-sdk/edit/master/website/',
          showLastUpdateTime: true,
        },
        blog: false,
        theme: {
          customCss: './src/css/custom.css',
        },
      }),
    ],
  ],

  plugins: [
    [
      '@easyops-cn/docusaurus-search-local',
      /** @type {import('@easyops-cn/docusaurus-search-local').PluginOptions} */
      ({
        hashed: true,
        indexBlog: false,
        docsRouteBasePath: '/docs',
        highlightSearchTermsOnTargetPage: true,
      }),
    ],
  ],

  themeConfig:
    /** @type {import('@docusaurus/preset-classic').ThemeConfig} */
    ({
      image: 'img/social-card.svg',
      colorMode: {
        defaultMode: 'light',
        respectPrefersColorScheme: true,
      },
      navbar: {
        title: 'Mobile SDK',
        logo: {
          alt: 'CARTO Mobile SDK',
          src: 'img/logo.svg',
        },
        items: [
          {
            type: 'docSidebar',
            sidebarId: 'docsSidebar',
            position: 'left',
            label: 'Documentation',
          },
          {to: '/docs/features/3d-terrain', label: 'Features', position: 'left'},
          {
            label: 'API Reference',
            position: 'left',
            items: [
              {label: 'Android (Javadoc)', href: 'pathname:///mobile-sdk/api/android/'},
              {label: 'iOS (Jazzy)', href: 'pathname:///mobile-sdk/api/ios/'},
            ],
          },
          {
            href: 'https://github.com/Akylas/mobile-sdk/releases',
            label: 'Releases',
            position: 'right',
          },
          {
            href: 'https://github.com/Akylas/mobile-sdk',
            label: 'GitHub',
            position: 'right',
          },
        ],
      },
      footer: {
        style: 'dark',
        links: [
          {
            title: 'Docs',
            items: [
              {label: 'Getting Started', to: '/docs/getting-started/installation'},
              {label: 'Guides', to: '/docs/guides/map-view'},
              {label: 'Features', to: '/docs/features/3d-terrain'},
            ],
          },
          {
            title: 'API',
            items: [
              {label: 'Android API', href: 'pathname:///mobile-sdk/api/android/'},
              {label: 'iOS API', href: 'pathname:///mobile-sdk/api/ios/'},
            ],
          },
          {
            title: 'More',
            items: [
              {label: 'GitHub', href: 'https://github.com/Akylas/mobile-sdk'},
              {label: 'Releases', href: 'https://github.com/Akylas/mobile-sdk/releases'},
              {label: 'Original CARTO docs', href: 'https://cartodb.github.io/developers/mobile-sdk/'},
            ],
          },
        ],
        copyright: `Copyright © ${new Date().getFullYear()} Akylas — maintained fork of CartoDB/mobile-sdk. Built with Docusaurus.`,
      },
      prism: {
        theme: prismThemes.github,
        darkTheme: prismThemes.dracula,
        additionalLanguages: ['java', 'kotlin', 'swift', 'objectivec', 'csharp', 'groovy', 'json', 'css', 'bash'],
      },
    }),
};

export default config;
