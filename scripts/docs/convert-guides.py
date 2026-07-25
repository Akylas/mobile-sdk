#!/usr/bin/env python3
"""Convert the vendored Jekyll-flavoured CARTO guides (docs/guides/*.md) into
Docusaurus MDX (website/docs/guides/*.mdx).

- `{% highlight LANG %}` / `{% endhighlight %}` -> fenced code blocks
- the original language tab groups (``<div class="js-tabs">`` … ``<div id="tab-java">`` …)
  become Docusaurus ``<Tabs>`` / ``<TabItem>`` with a shared ``groupId`` so the
  reader's language choice syncs across the whole page
- strips remaining Liquid tags / kramdown attr lists, rewrites image paths
- MDX-escapes prose (``{`` / ``<`` outside code) so the vendored content parses under
  the strict MDX compiler; inline-code and fenced code are left verbatim

Feature-specific and install pages are authored by hand (as .md / CommonMark).
"""
import os
import re

SRC = os.path.join(os.path.dirname(__file__), "..", "..", "docs", "guides")
DST = os.path.join(os.path.dirname(__file__), "..", "..", "website", "docs", "guides")

# NN prefix -> (slug, Title). sidebar_position = NN.
TITLES = {
    "01": ("getting-started", "Getting Started"),
    "02": ("map-view", "Map View"),
    "03": ("layers-and-data-sources", "Layers & Data Sources"),
    "04": ("vector-objects", "Vector Objects on the Map"),
    "05": ("offline-maps", "Offline Maps"),
    "06": ("package-manager", "Package Manager"),
    "07": ("geocoding", "Geocoding"),
    "08": ("routing", "Routing"),
    "09": ("carto-integrations", "CARTO Integrations"),
    "10": ("clustering", "Clustering"),
    "11": ("ground-overlays", "Ground Overlays"),
    "12": ("performance", "Performance"),
    "13": ("api-keys", "API Keys"),
}

HIGHLIGHT_OPEN = re.compile(r"\{%\s*highlight\s+([A-Za-z0-9_+-]+)[^%]*%\}")
HIGHLIGHT_CLOSE = re.compile(r"\{%\s*endhighlight\s*%\}")
LIQUID = re.compile(r"\{%[^%]*%\}")
LIQUID_VAR = re.compile(r"\{\{[^}]*\}\}")
KRAMDOWN_ATTR = re.compile(r"^\s*\{:[^}]*\}\s*$", re.M)
IMG_PATH = re.compile(r"(\]\()(?:\.\./)+img/")
LANG_MAP = {"objc": "objectivec", "obj-c": "objectivec", "cs": "csharp", "cpp": "cpp"}

TABS_OPEN = re.compile(r'^\s*<div\s+class="js-tabs[^"]*"\s*>\s*$', re.I)
UL_OPEN = re.compile(r'^\s*<ul\s+class="tab-navigation"\s*>\s*$', re.I)
UL_CLOSE = re.compile(r"^\s*</ul>\s*$", re.I)
NAV_ANCHOR = re.compile(r'<a\s+href="#tab-([^"]+)"\s*>(.*?)</a>', re.I)
# Matches <div id="tab-java"> and the occasional source typo <div id="">.
ITEM_OPEN = re.compile(r'^\s*<div\s+id="(?:tab-)?([^"]*)"\s*>\s*$', re.I)
FENCE_LANG = re.compile(r"^\s*```([A-Za-z0-9_+-]+)")
DIV_CLOSE = re.compile(r"^\s*</div>\s*$", re.I)
LI_ANY = re.compile(r"^\s*</?li[^>]*>\s*$", re.I)

DEFAULT_LABELS = {
    "java": "Java",
    "kotlin": "Kotlin",
    "csharp": "C#",
    "objectivec": "Objective-C",
    "swift": "Swift",
}


def pre_clean(text: str) -> str:
    text = HIGHLIGHT_OPEN.sub(
        lambda m: "```" + LANG_MAP.get(m.group(1).lower(), m.group(1).lower()), text
    )
    text = HIGHLIGHT_CLOSE.sub("```", text)
    text = LIQUID.sub("", text)
    text = LIQUID_VAR.sub("#", text)
    text = KRAMDOWN_ATTR.sub("", text)
    text = IMG_PATH.sub(r"\1/img/", text)
    return text


def dedent(lines):
    indents = [len(l) - len(l.lstrip()) for l in lines if l.strip()]
    n = min(indents) if indents else 0
    return [l[n:] if len(l) >= n else l for l in lines]


def mdx_escape(line: str) -> str:
    """Escape MDX-hostile chars in a prose line, protecting inline-code spans."""
    parts = re.split(r"(`+[^`]*`+)", line)
    for i in range(0, len(parts), 2):  # even indices are outside inline code
        p = parts[i]
        p = p.replace("{", "&#123;").replace("}", "&#125;")
        p = p.replace("<", "&lt;")
        parts[i] = p
    return "".join(parts)


def emit_content(lines, out):
    """Append content lines to out, escaping prose but leaving fenced code verbatim."""
    in_fence = False
    for line in lines:
        if line.lstrip().startswith("```"):
            in_fence = not in_fence
            out.append(line)
        elif in_fence:
            out.append(line)
        else:
            out.append(mdx_escape(line))


def transform(text: str) -> str:
    lines = text.split("\n")
    out = []
    i = 0
    n = len(lines)
    fence = False  # top-level fence tracking (outside tab regions)
    while i < n:
        line = lines[i]

        # Passthrough fenced code verbatim at top level.
        if fence:
            out.append(line)
            if line.lstrip().startswith("```"):
                fence = False
            i += 1
            continue
        if line.lstrip().startswith("```"):
            out.append(line)
            fence = True
            i += 1
            continue

        # Start of a tab group.
        if TABS_OPEN.match(line):
            labels = {}
            i += 1
            # Parse the nav <ul> for labels.
            while i < n and not UL_CLOSE.match(lines[i]):
                if UL_OPEN.match(lines[i]) or LI_ANY.match(lines[i]):
                    i += 1
                    continue
                m = NAV_ANCHOR.search(lines[i])
                if m:
                    labels[m.group(1).lower()] = m.group(2).strip()
                i += 1
            i += 1  # skip </ul>

            out.append("")
            out.append('<Tabs groupId="language">')
            # Parse each <div id="tab-X"> … </div>. Items are flat (one closing
            # </div> each), so the first </div> seen at this level closes the
            # <Tabs> wrapper.
            while i < n and not DIV_CLOSE.match(lines[i]):
                mi = ITEM_OPEN.match(lines[i])
                if not mi:
                    i += 1  # stray blank line between items
                    continue
                value = mi.group(1).lower()
                i += 1
                buf = []
                while i < n and not DIV_CLOSE.match(lines[i]):
                    buf.append(lines[i])
                    i += 1
                i += 1  # consume the item's </div>
                if not value:  # source typo id="": infer from the first code fence
                    for b in buf:
                        fm = FENCE_LANG.match(b)
                        if fm:
                            value = LANG_MAP.get(fm.group(1).lower(), fm.group(1).lower())
                            break
                label = labels.get(value) or DEFAULT_LABELS.get(value, value or "code")
                out.append("")
                out.append(f'<TabItem value="{value or "code"}" label="{label}">')
                out.append("")
                emit_content(dedent(buf), out)
                out.append("")
                out.append("</TabItem>")
            i += 1  # skip the closing </div> of the tabs wrapper
            out.append("")
            out.append("</Tabs>")
            out.append("")
            continue

        # Ordinary prose line.
        out.append(mdx_escape(line))
        i += 1

    text = "\n".join(out)
    text = re.sub(r"\n{3,}", "\n\n", text)
    return text.strip() + "\n"


def main():
    os.makedirs(DST, exist_ok=True)
    # Clear any previous output for these slugs (.md or .mdx).
    for slug, _ in TITLES.values():
        for ext in (".md", ".mdx"):
            p = os.path.join(DST, slug + ext)
            if os.path.exists(p):
                os.remove(p)

    for fn in sorted(os.listdir(SRC)):
        if not fn.endswith(".md") or fn[:2] not in TITLES:
            continue
        slug, title = TITLES[fn[:2]]
        with open(os.path.join(SRC, fn), encoding="utf-8") as fh:
            body = transform(pre_clean(fh.read()))
        fm = (
            "---\n"
            f"title: {title}\n"
            f"sidebar_position: {int(fn[:2])}\n"
            f"slug: /guides/{slug}\n"
            "---\n\n"
            "import Tabs from '@theme/Tabs';\n"
            "import TabItem from '@theme/TabItem';\n\n"
        )
        out = os.path.join(DST, f"{slug}.mdx")
        with open(out, "w", encoding="utf-8") as fh:
            fh.write(fm + body)
        print(f"  {fn} -> {os.path.relpath(out)}")


if __name__ == "__main__":
    main()
