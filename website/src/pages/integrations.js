import Layout from '@theme/Layout';
import Link from '@docusaurus/Link';
import CodeBlock from '@theme/CodeBlock';
import Ticked from '@site/src/components/Ticked';
import {Integrations} from '@site/src/data/integrations';

function IntegrationCard({integration}) {
  const {icon, name, status, pkg, version, install, repo, npm, platforms, body, note} = integration;
  const planned = status === 'planned';
  return (
    <div className="integrationCard">
      <div className="integrationCardHead">
        <span className="integrationCardIcon">{icon}</span>
        <div>
          <h3>{name}</h3>
          {pkg && (
            <div className="integrationCardPkg">
              <code>{pkg}</code> <span className="integrationCardVersion">v{version}</span>
            </div>
          )}
        </div>
        <span className={`statusBadge ${planned ? 'statusPlanned' : 'statusSupported'}`}>
          {planned ? 'Coming' : 'Available'}
        </span>
      </div>
      <p><Ticked>{body}</Ticked></p>
      <p className="integrationCardPlatforms">
        Targets: {platforms.join(' · ')}
      </p>
      {install && <CodeBlock language="bash">{install}</CodeBlock>}
      {note && <p className="integrationCardNote"><Ticked>{note}</Ticked></p>}
      {(repo || npm) && (
        <div className="integrationCardLinks">
          {repo && <a href={repo}>Source →</a>}
          {npm && <a href={npm}>npm →</a>}
        </div>
      )}
    </div>
  );
}

export default function IntegrationsPage() {
  return (
    <Layout
      title="Integrations"
      description="Framework plugins that expose Massif Maps outside native Android and iOS.">
      <header className="pageHeader">
        <div className="container">
          <h1>Integrations</h1>
          <p>
            The SDK is C++ with native bindings. These plugins put it inside a cross-platform
            framework — they wrap the published Android and iOS artifacts, they are not separate
            renderers.
          </p>
        </div>
      </header>

      <main className="container margin-vert--xl">
        {Integrations.map((i) => (
          <IntegrationCard key={i.id} integration={i} />
        ))}

        <div className="customWorkBox margin-top--xl">
          <h3>Maintaining an integration?</h3>
          <p>
            Open an issue and it gets listed here. If the framework you need is missing,{' '}
            <Link to="/sponsors">sponsoring</Link> is what moves it up the{' '}
            <Link to="/roadmap">roadmap</Link>.
          </p>
          <Link className="button button--primary" to="/community">
            Get in touch
          </Link>
        </div>
      </main>
    </Layout>
  );
}
