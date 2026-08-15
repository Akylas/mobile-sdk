import React, {useEffect, useState} from 'react';
import Layout from '@theme/Layout';
import {usePluginData} from '@docusaurus/useGlobalData';

const RAW_URL =
  'https://raw.githubusercontent.com/massif-maps/MassifMaps/master/CHANGELOG.md';

const GITHUB_URL =
  'https://github.com/massif-maps/MassifMaps/blob/master/CHANGELOG.md';

/**
 * Minimal markdown-to-HTML converter tailored to the CHANGELOG.md format.
 * Handles: ATX headings, horizontal rules, fenced code blocks, blockquotes,
 * unordered lists, bold/italic/inline-code, and links.
 */
function markdownToHtml(md) {
  const lines = md.split('\n');
  const out = [];
  let i = 0;
  let inList = false;
  let inCode = false;
  let codeLang = '';
  let codeLines = [];

  const closeList = () => {
    if (inList) {
      out.push('</ul>');
      inList = false;
    }
  };

  const inlineHtml = (text) =>
    text
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/`([^`]+)`/g, '<code>$1</code>')
      .replace(/\*\*([^*]+)\*\*/g, '<strong>$1</strong>')
      .replace(/\*([^*]+)\*/g, '<em>$1</em>')
      .replace(/\[([^\]]+)\]\(([^)]+)\)/g, '<a href="$2">$1</a>');

  while (i < lines.length) {
    const line = lines[i];

    // Fenced code block start/end
    if (line.startsWith('```')) {
      if (inCode) {
        out.push(
          `<pre><code${codeLang ? ` class="language-${codeLang}"` : ''}>${codeLines
            .join('\n')
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;')}</code></pre>`,
        );
        inCode = false;
        codeLines = [];
        codeLang = '';
      } else {
        closeList();
        inCode = true;
        codeLang = line.slice(3).trim();
      }
      i++;
      continue;
    }

    if (inCode) {
      codeLines.push(line);
      i++;
      continue;
    }

    // Headings
    const h3 = line.match(/^### (.+)/);
    const h2 = line.match(/^## (.+)/);
    const h1 = line.match(/^# (.+)/);
    if (h1) {
      closeList();
      out.push(`<h1>${inlineHtml(h1[1])}</h1>`);
      i++;
      continue;
    }
    if (h2) {
      closeList();
      out.push(`<h2>${inlineHtml(h2[1])}</h2>`);
      i++;
      continue;
    }
    if (h3) {
      closeList();
      out.push(`<h3>${inlineHtml(h3[1])}</h3>`);
      i++;
      continue;
    }

    // Horizontal rule
    if (/^---+$/.test(line.trim())) {
      closeList();
      out.push('<hr/>');
      i++;
      continue;
    }

    // Unordered list item (including nested `  -`)
    const li = line.match(/^(\s*)[-*] (.+)/);
    if (li) {
      if (!inList) {
        out.push('<ul>');
        inList = true;
      }
      const indent = li[1].length;
      out.push(
        `<li style="margin-left:${indent * 12}px">${inlineHtml(li[2])}</li>`,
      );
      i++;
      continue;
    }

    // Blockquote
    const bq = line.match(/^> (.+)/);
    if (bq) {
      closeList();
      out.push(`<blockquote><p>${inlineHtml(bq[1])}</p></blockquote>`);
      i++;
      continue;
    }

    // Blank line
    if (line.trim() === '') {
      closeList();
      out.push('<br/>');
      i++;
      continue;
    }

    // Paragraph / plain text
    closeList();
    out.push(`<p>${inlineHtml(line)}</p>`);
    i++;
  }

  closeList();
  return out.join('\n');
}

export default function ChangelogPage() {
  const buildData = usePluginData('changelog-content');
  const buildContent = buildData && buildData.content;

  const [content, setContent] = useState(buildContent || null);
  const [loading, setLoading] = useState(!buildContent);
  const [error, setError] = useState(null);

  useEffect(() => {
    if (content) return; // already have build-time content
    fetch(RAW_URL)
      .then((r) => {
        if (!r.ok) throw new Error(`HTTP ${r.status}`);
        return r.text();
      })
      .then((text) => {
        setContent(text);
        setLoading(false);
      })
      .catch((err) => {
        setError(err.message);
        setLoading(false);
      });
  }, [content]);

  return (
    <Layout
      title="Changelog"
      description="All notable changes to Massif Maps, organized by release."
    >
      <main className="container margin-vert--lg">
        <div className="row">
          <div className="col col--10 col--offset-1">
            <div
              style={{
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center',
                marginBottom: '1rem',
              }}
            >
              <h1 style={{margin: 0}}>Changelog</h1>
              <a
                href={GITHUB_URL}
                target="_blank"
                rel="noreferrer"
                className="button button--secondary button--sm"
              >
                View on GitHub →
              </a>
            </div>

            {loading && (
              <p className="text--secondary">Loading changelog…</p>
            )}

            {error && (
              <div className="admonition admonition-warning alert alert--warning">
                <p>
                  Could not load the changelog from GitHub ({error}).{' '}
                  <a href={GITHUB_URL} target="_blank" rel="noreferrer">
                    View it on GitHub directly.
                  </a>
                </p>
              </div>
            )}

            {content && (
              <div
                className="markdown"
                // eslint-disable-next-line react/no-danger
                dangerouslySetInnerHTML={{__html: markdownToHtml(content)}}
              />
            )}
          </div>
        </div>
      </main>
    </Layout>
  );
}
