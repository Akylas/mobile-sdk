import Layout from '@theme/Layout';
import Link from '@docusaurus/Link';
import Ticked from '@site/src/components/Ticked';
import {Channels, Repos, Contributing} from '@site/src/data/community';

export default function CommunityPage() {
  return (
    <Layout
      title="Community"
      description="Where to ask, report and contribute — Massif Maps runs on GitHub issues and discussions.">
      <header className="pageHeader">
        <div className="container">
          <h1>Community</h1>
          <p>
            Everything happens on GitHub. No forum, no chat server to keep alive — issues and
            discussions are the whole surface, and that is also what feeds the{' '}
            <Link to="/roadmap">roadmap</Link>.
          </p>
        </div>
      </header>

      <main className="container margin-vert--xl">
        <div className="row">
          {Channels.map((c) => (
            <div key={c.title} className="col col--4" style={{marginBottom: '1.5rem'}}>
              <div className="communityCard">
                <div className="communityCardIcon">{c.icon}</div>
                <h3>{c.title}</h3>
                <p><Ticked>{c.body}</Ticked></p>
                <a className="button button--primary button--block" href={c.cta.href}>
                  {c.cta.label}
                </a>
              </div>
            </div>
          ))}
        </div>

        <h2 className="margin-top--xl">The repositories</h2>
        <p className="sectionLead">
          One fork, two submodules. Work in a submodule needs its own branch, commit and PR —
          the pointer bump in the main repo comes second.
        </p>
        <div className="row">
          {Repos.map((r) => (
            <div key={r.name} className="col col--4" style={{marginBottom: '1.5rem'}}>
              <div className="communityCard">
                <h3>
                  <a href={r.href}>{r.name}</a>
                </h3>
                <p className="repoBranch">
                  base branch <code>{r.branch}</code>
                </p>
                <p><Ticked>{r.body}</Ticked></p>
              </div>
            </div>
          ))}
        </div>

        <h2 className="margin-top--xl">Contributing</h2>
        <p className="sectionLead">
          Pull requests are welcome, including on the platforms marked unmaintained on the{' '}
          <Link to="/platforms">platforms page</Link>. Three things to know before the first one:
        </p>
        <div className="row">
          {Contributing.map((c) => (
            <div key={c.title} className="col col--4" style={{marginBottom: '1.5rem'}}>
              <div className="communityCard">
                <h3>{c.title}</h3>
                <p><Ticked>{c.body}</Ticked></p>
                <a href={c.href}>{c.linkLabel} →</a>
              </div>
            </div>
          ))}
        </div>

        <h2 className="margin-top--xl">Licence</h2>
        <p>
          BSD 3-Clause, like the original CARTO SDK. No licence key, no registration, no online
          service to sign up for — see{' '}
          <Link to="/docs/migration">Migrating to Massif Maps</Link> if you are coming from the
          CARTO 4.x API.
        </p>
      </main>
    </Layout>
  );
}
