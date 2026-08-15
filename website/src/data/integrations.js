/**
 * Framework integrations — wrappers that expose the SDK to a non-native
 * framework. `status`: 'available' | 'planned'.
 */

export const Integrations = [
  {
    id: 'nativescript',
    name: 'NativeScript',
    icon: '🟦',
    status: 'available',
    pkg: '@nativescript-community/ui-massifmaps',
    version: '2.1.0',
    install: 'npm install @nativescript-community/ui-massifmaps',
    repo: 'https://github.com/nativescript-community/ui-massifmaps',
    npm: 'https://www.npmjs.com/package/@nativescript-community/ui-massifmaps',
    platforms: ['Android', 'iOS'],
    body:
      'Full map view for NativeScript with Vue, Svelte, Angular and plain TypeScript. Wraps the ' +
      'Android and iOS artifacts, so every renderer feature is reachable from JavaScript.',
    note:
      'Renamed from `@nativescript-community/ui-carto` — update the import path when you move to 2.x.',
  },
  {
    id: 'flutter',
    name: 'Flutter',
    icon: '🐦',
    status: 'planned',
    platforms: ['Android', 'iOS'],
    body:
      'A Dart binding over the published native artifacts, using platform views for the map ' +
      'surface. Tracked on the roadmap.',
  },
  {
    id: 'react-native',
    name: 'React Native',
    icon: '⚛️',
    status: 'planned',
    platforms: ['Android', 'iOS'],
    body:
      'A turbo-module wrapper, same shape as the Flutter one — no second renderer, just a ' +
      'binding over the native builds.',
  },
];
