#!/usr/bin/env python3
"""Convert the vendored Jekyll-flavoured CARTO guides (docs/guides/*.md) into
clean CommonMark for the Docusaurus site (website/docs/guides/*.md).

- `{% highlight LANG ... %}` / `{% endhighlight %}` -> fenced code blocks
- strips remaining Liquid tags ({% ... %}) and kramdown attr lists ({: ... })
- rewrites image paths ../../img/x -> /img/x
- injects Docusaurus frontmatter (title + sidebar_position) from the NN- filename prefix

This is a mechanical migration; feature-specific and install pages are authored by hand.
"""
import os
import re
import sys

SRC = os.path.join(os.path.dirname(__file__), "..", "..", "docs", "guides")
DST = os.path.join(os.path.dirname(__file__), "..", "..", "website", "docs", "guides")

# NN- prefix -> (slug, Title). Order preserved via sidebar_position = NN.
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
# Liquid variables like {{ site.sqlapi_docs }} — neutralize so they don't become
# broken links. In link/URL position they collapse to a harmless "#" anchor.
LIQUID_VAR = re.compile(r"\{\{[^}]*\}\}")
KRAMDOWN_ATTR = re.compile(r"^\s*\{:[^}]*\}\s*$", re.M)
IMG_PATH = re.compile(r"(\]\()(?:\.\./)+img/")
LANG_MAP = {"objc": "objectivec", "obj-c": "objectivec", "cs": "csharp", "cpp": "cpp"}


# Language-tab scaffolding from the original docs. The fenced code blocks inside
# each tab keep their own language label, so we drop the tab chrome and let the
# code blocks stack (Docusaurus shows the language on each block).
TAB_LINE = re.compile(
    r"^\s*(</?div\b[^>]*>|</?ul\b[^>]*>|</?li\b[^>]*>|<a\s+href=\"#tab-[^\"]*\"[^>]*>.*?</a>)\s*$",
    re.I,
)


def strip_tab_chrome(text: str) -> str:
    """Remove tab-navigation HTML that lives *outside* fenced code blocks."""
    out, in_fence = [], False
    for line in text.split("\n"):
        if line.lstrip().startswith("```"):
            in_fence = not in_fence
            out.append(line)
            continue
        if not in_fence and TAB_LINE.match(line):
            continue
        out.append(line)
    return "\n".join(out)


def convert(text: str) -> str:
    text = HIGHLIGHT_OPEN.sub(lambda m: "```" + LANG_MAP.get(m.group(1).lower(), m.group(1).lower()), text)
    text = HIGHLIGHT_CLOSE.sub("```", text)
    text = LIQUID.sub("", text)              # drop remaining liquid tags
    text = LIQUID_VAR.sub("#", text)         # neutralize {{ site.* }} variables
    text = KRAMDOWN_ATTR.sub("", text)       # drop {: .class } attribute lists
    text = IMG_PATH.sub(r"\1/img/", text)    # ../../img/x -> /img/x
    text = strip_tab_chrome(text)            # drop <div>/<ul>/<li>/<a #tab-*> chrome
    # collapse 3+ blank lines
    text = re.sub(r"\n{3,}", "\n\n", text)
    return text.strip() + "\n"


def main():
    os.makedirs(DST, exist_ok=True)
    for fn in sorted(os.listdir(SRC)):
        if not fn.endswith(".md"):
            continue
        prefix = fn[:2]
        if prefix not in TITLES:
            continue
        slug, title = TITLES[prefix]
        with open(os.path.join(SRC, fn), encoding="utf-8") as fh:
            body = convert(fh.read())
        fm = (
            "---\n"
            f"title: {title}\n"
            f"sidebar_position: {int(prefix)}\n"
            f"slug: /guides/{slug}\n"
            "---\n\n"
        )
        out = os.path.join(DST, f"{slug}.md")
        with open(out, "w", encoding="utf-8") as fh:
            fh.write(fm + body)
        print(f"  {fn} -> {os.path.relpath(out)}")


if __name__ == "__main__":
    main()
