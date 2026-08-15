/**
 * changelog-content plugin — reads CHANGELOG.md from the repo root at build
 * time and exposes its text as Docusaurus global plugin data so the
 * /changelog page can render it without a runtime network call.
 *
 * Falls back gracefully when the file is not present (e.g. shallow checkouts).
 */

const fs = require('fs');
const path = require('path');

module.exports = function changelogContentPlugin(_context, _options) {
  return {
    name: 'changelog-content',

    async loadContent() {
      const changelogPath = path.resolve(__dirname, '../../../CHANGELOG.md');
      try {
        return {content: fs.readFileSync(changelogPath, 'utf8')};
      } catch {
        return {content: null};
      }
    },

    async contentLoaded({content, actions}) {
      actions.setGlobalData(content);
    },
  };
};
