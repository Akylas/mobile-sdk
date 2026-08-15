/**
 * Roadmap plugin — the /roadmap page is the `roadmap`-labelled issues of the
 * SDK repo, fetched once at build time and exposed as plugin global data.
 *
 * Auth: set GITHUB_TOKEN (CI does) to lift the 60 req/h anonymous limit.
 * When the API is unreachable or rate-limited the build does NOT fail — it
 * falls back to `src/data/roadmap-fallback.json` so an offline `npm run build`
 * still produces a page. Freshness therefore depends on the nightly rebuild in
 * `.github/workflows/docs.yml`.
 */

const fs = require('fs');
const path = require('path');

const DEFAULTS = {
  owner: 'massif-maps',
  repo: 'MassifMaps',
  label: 'roadmap',
  // Issue labels that place a card in a column, most-advanced first.
  statusLabels: [
    {label: 'status:in-progress', id: 'in-progress', title: 'In progress'},
    {label: 'status:next', id: 'next', title: 'Next up'},
  ],
  fallbackColumn: {id: 'considering', title: 'Exploring'},
  doneColumn: {id: 'done', title: 'Shipped'},
  fallbackFile: 'src/data/roadmap-fallback.json',
};

/** First image in an issue body: markdown `![](url)` or a raw <img src>. */
function firstImage(body) {
  if (!body) return null;
  const md = body.match(/!\[[^\]]*\]\(\s*<?([^)\s>]+)>?[^)]*\)/);
  if (md) return md[1];
  const html = body.match(/<img[^>]+src=["']([^"']+)["']/i);
  if (html) return html[1];
  return null;
}

/** Body with the image markup and HTML comments removed, trimmed to a teaser. */
function summarize(body, limit = 260) {
  if (!body) return '';
  const text = body
    .replace(/<!--[\s\S]*?-->/g, '')
    .replace(/!\[[^\]]*\]\([^)]*\)/g, '')
    .replace(/<img[^>]*>/gi, '')
    .replace(/```[\s\S]*?```/g, '')
    .replace(/^#{1,6}\s+/gm, '')
    .replace(/\r/g, '')
    .split('\n')
    .map((l) => l.trim())
    .filter(Boolean)
    .join(' ')
    .trim();
  return text.length > limit ? `${text.slice(0, limit).trimEnd()}…` : text;
}

function columnFor(issue, options) {
  if (issue.state === 'closed') return options.doneColumn.id;
  const names = (issue.labels || []).map((l) => (typeof l === 'string' ? l : l.name));
  const match = options.statusLabels.find((s) => names.includes(s.label));
  return match ? match.id : options.fallbackColumn.id;
}

function toCard(issue, options) {
  return {
    number: issue.number,
    title: issue.title,
    url: issue.html_url,
    state: issue.state,
    image: firstImage(issue.body),
    summary: summarize(issue.body),
    column: columnFor(issue, options),
    comments: issue.comments,
    reactions: issue.reactions ? issue.reactions.total_count : 0,
    labels: (issue.labels || [])
      .map((l) => (typeof l === 'string' ? l : l.name))
      .filter((n) => n !== options.label && !n.startsWith('status:')),
    updatedAt: issue.updated_at,
  };
}

async function fetchIssues(options) {
  const {owner, repo, label} = options;
  const headers = {
    accept: 'application/vnd.github+json',
    'user-agent': 'massif-maps-website',
  };
  const token = process.env.GITHUB_TOKEN || process.env.GH_TOKEN;
  if (token) headers.authorization = `Bearer ${token}`;

  const url =
    `https://api.github.com/repos/${owner}/${repo}/issues` +
    `?labels=${encodeURIComponent(label)}&state=all&per_page=100&sort=updated&direction=desc`;

  const res = await fetch(url, {headers});
  if (!res.ok) {
    throw new Error(`GitHub API ${res.status} ${res.statusText}`);
  }
  const issues = await res.json();
  // The issues endpoint also returns pull requests; drop them.
  return issues.filter((i) => !i.pull_request);
}

module.exports = function roadmapIssuesPlugin(context, opts = {}) {
  const options = {...DEFAULTS, ...opts};
  const fallbackPath = path.join(context.siteDir, options.fallbackFile);

  return {
    name: 'massif-roadmap',

    async loadContent() {
      let issues;
      let stale = false;
      try {
        issues = await fetchIssues(options);
        try {
          fs.writeFileSync(fallbackPath, `${JSON.stringify(issues, null, 2)}\n`);
        } catch (e) {
          console.warn(`[massif-roadmap] could not refresh the fallback cache: ${e.message}`);
        }
      } catch (e) {
        console.warn(`[massif-roadmap] ${e.message} — falling back to ${options.fallbackFile}`);
        stale = true;
        try {
          issues = JSON.parse(fs.readFileSync(fallbackPath, 'utf8'));
        } catch (_) {
          issues = [];
        }
      }

      const cards = issues.map((i) => toCard(i, options));
      const columns = [
        ...options.statusLabels.map(({id, title}) => ({id, title})),
        options.fallbackColumn,
        options.doneColumn,
      ].map((col) => ({...col, cards: cards.filter((c) => c.column === col.id)}));

      return {
        repo: `${options.owner}/${options.repo}`,
        label: options.label,
        issuesUrl: `https://github.com/${options.owner}/${options.repo}/issues?q=is%3Aissue+label%3A${options.label}`,
        newIssueUrl: `https://github.com/${options.owner}/${options.repo}/issues/new`,
        stale,
        total: cards.length,
        columns,
      };
    },

    async contentLoaded({content, actions}) {
      actions.setGlobalData(content);
    },
  };
};
