import Layout from '@theme/Layout';
import Link from '@docusaurus/Link';
import {usePluginData} from '@docusaurus/useGlobalData';

function Card({card}) {
  return (
    <a className="roadmapCard" href={card.url} target="_blank" rel="noreferrer">
      {card.image && (
        <div className="roadmapCardImage">
          <img src={card.image} alt="" loading="lazy" />
        </div>
      )}
      <div className="roadmapCardBody">
        <div className="roadmapCardTitle">{card.title}</div>
        {card.summary && <p className="roadmapCardSummary">{card.summary}</p>}
        <div className="roadmapCardFooter">
          <span className="roadmapCardNumber">#{card.number}</span>
          {card.labels.map((l) => (
            <span key={l} className="roadmapCardLabel">
              {l}
            </span>
          ))}
          {card.reactions > 0 && <span className="roadmapCardStat">👍 {card.reactions}</span>}
          {card.comments > 0 && <span className="roadmapCardStat">💬 {card.comments}</span>}
        </div>
      </div>
    </a>
  );
}

function Column({column}) {
  return (
    <section className="roadmapColumn">
      <h2 className="roadmapColumnTitle">
        {column.title} <span className="roadmapColumnCount">{column.cards.length}</span>
      </h2>
      {column.cards.length === 0 ? (
        <p className="roadmapColumnEmpty">Nothing here yet.</p>
      ) : (
        column.cards.map((card) => <Card key={card.number} card={card} />)
      )}
    </section>
  );
}

export default function RoadmapPage() {
  const data = usePluginData('massif-roadmap');
  const columns = (data.columns || []).filter((c) => c.cards.length > 0);

  return (
    <Layout
      title="Roadmap"
      description="What is planned, in progress and shipped in Massif Maps — generated from the roadmap-labelled GitHub issues.">
      <header className="pageHeader">
        <div className="container">
          <h1>Roadmap</h1>
          <p>
            There is no separate roadmap document. Every item below is a GitHub issue labelled{' '}
            <code>{data.label}</code> in <code>{data.repo}</code> — comment on one to push it up,
            or <a href={data.newIssueUrl}>open a new issue</a> to propose something.
          </p>
        </div>
      </header>

      <main className="container margin-vert--xl">
        {data.stale && (
          <div className="admonition admonition-caution alert alert--warning margin-bottom--lg">
            <div className="admonition-content">
              This page was built without reaching the GitHub API, so it may be out of date.{' '}
              <a href={data.issuesUrl}>See the live issue list</a>.
            </div>
          </div>
        )}

        {data.total === 0 ? (
          <div className="roadmapEmpty">
            <h2>No roadmap items yet</h2>
            <p>
              Nothing carries the <code>{data.label}</code> label right now. The page fills itself
              as soon as issues get labelled — title, first image and body teaser come straight
              from the issue.
            </p>
            <Link className="button button--primary" to={data.newIssueUrl}>
              Propose something
            </Link>
          </div>
        ) : (
          <div className="roadmapBoard">
            {columns.map((column) => (
              <Column key={column.id} column={column} />
            ))}
          </div>
        )}

        <p className="margin-top--xl roadmapFootnote">
          Rebuilt nightly from GitHub. <a href={data.issuesUrl}>Browse the issues directly →</a>{' '}
          Sponsoring changes the order — see <Link to="/sponsors">Sponsors</Link>.
        </p>
      </main>
    </Layout>
  );
}
