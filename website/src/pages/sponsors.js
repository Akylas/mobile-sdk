import Layout from '@theme/Layout';
import Link from '@docusaurus/Link';
import useBaseUrl from '@docusaurus/useBaseUrl';
import {
  Tiers,
  Sponsors,
  Channels,
  Badges,
  CustomWork,
  SponsorContact,
} from '@site/src/data/sponsors';

function SponsorLogo({sponsor, className}) {
  return (
    <a href={sponsor.url} target="_blank" rel="noreferrer" className={className}>
      <img src={useBaseUrl(sponsor.logo)} alt={sponsor.name} />
    </a>
  );
}

function ChannelCard({channel}) {
  const {icon, title, body, cta, available} = channel;
  return (
    <div className="col col--4" style={{marginBottom: '1.5rem'}}>
      <div className={`sponsorChannel${available ? '' : ' sponsorChannelSoon'}`}>
        <div className="sponsorChannelIcon">{icon}</div>
        <h3>{title}</h3>
        <p>{body}</p>
        {cta.href ? (
          <a className="button button--primary button--block" href={cta.href}>
            {cta.label}
          </a>
        ) : (
          <span className="button button--secondary button--block disabled">{cta.label}</span>
        )}
      </div>
    </div>
  );
}

function TierCard({tier}) {
  const sponsors = Sponsors.filter((s) => s.tier === tier.id);
  return (
    <div className="col col--3" style={{marginBottom: '1.5rem'}}>
      <div className="tierCard" style={{'--tier-accent': tier.accent}}>
        <div className="tierCardName">{tier.name}</div>
        <div className="tierCardAmount">{tier.amount}</div>
        <ul className="tierCardBenefits">
          {tier.benefits.map((b) => (
            <li key={b}>{b}</li>
          ))}
        </ul>
        <a
          className="button button--outline button--primary button--block"
          href={`mailto:${SponsorContact}?subject=Massif%20Maps%20sponsoring%20—%20${tier.name}`}>
          Become {tier.name}
        </a>
        {sponsors.length > 0 && (
          <div className="tierCardSponsors">
            {sponsors.map((s) => (
              <SponsorLogo key={s.name} sponsor={s} />
            ))}
          </div>
        )}
      </div>
    </div>
  );
}

function BadgeRow({badge}) {
  const url = useBaseUrl(badge.file);
  const markdown = `[![${badge.label}](https://massif-maps.github.io${url})](https://massif-maps.github.io/MassifMaps/)`;
  return (
    <div className="badgeRow">
      <img src={url} alt={badge.label} />
      <code>{markdown}</code>
    </div>
  );
}

export default function SponsorsPage() {
  return (
    <Layout
      title="Sponsors"
      description="Fund Massif Maps — company tiers, GitHub Sponsors for individuals, and what sponsoring buys.">
      <header className="pageHeader">
        <div className="container">
          <h1>Sponsors</h1>
          <p>
            Massif Maps is open source and maintained by one small team. There is no licence fee
            and no hosted service to upsell — sponsoring is what pays for the work, and it is what
            decides the order of the <Link to="/roadmap">roadmap</Link>.
          </p>
        </div>
      </header>

      <main className="container margin-vert--xl">
        <h2>How to support the project</h2>
        <div className="row">
          {Channels.map((c) => (
            <ChannelCard key={c.id} channel={c} />
          ))}
        </div>

        <h2 className="margin-top--xl">Company tiers</h2>
        <p className="sectionLead">
          Yearly, invoiced. Email <a href={`mailto:${SponsorContact}`}>{SponsorContact}</a> and say
          which tier — we answer with an invoice and a logo request.
        </p>
        <div className="row">
          {Tiers.map((t) => (
            <TierCard key={t.id} tier={t} />
          ))}
        </div>

        <div className="customWorkBox margin-top--lg">
          <h3>{CustomWork.title}</h3>
          <p>{CustomWork.body}</p>
          <a
            className="button button--primary"
            href={`mailto:${SponsorContact}?subject=Massif%20Maps%20—%20funded%20feature`}>
            Ask for a quote
          </a>
        </div>

        <h2 className="margin-top--xl">Our sponsors</h2>
        {Sponsors.length === 0 ? (
          <div className="sponsorWallEmpty">
            <p>
              No sponsors yet — this wall is waiting for its first logo. Take it and it stays at
              the top of the page for a year.
            </p>
            <a className="button button--primary" href={`mailto:${SponsorContact}?subject=Massif%20Maps%20sponsoring`}>
              Be the first
            </a>
          </div>
        ) : (
          <div className="sponsorWall">
            {Sponsors.map((s) => (
              <SponsorLogo key={s.name} sponsor={s} className={`sponsorLogo sponsorLogo--${s.tier}`} />
            ))}
          </div>
        )}

        <h2 className="margin-top--xl">Badges</h2>
        <p className="sectionLead">
          Shipping something built on the SDK, or sponsoring it? Put a badge in your README.
        </p>
        {Badges.map((b) => (
          <BadgeRow key={b.id} badge={b} />
        ))}
      </main>
    </Layout>
  );
}
