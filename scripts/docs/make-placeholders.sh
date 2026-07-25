#!/usr/bin/env bash
# Generate placeholder feature images + social card (SVG) for the docs site.
# Real captures should replace website/static/img/features/* — see
# scripts/docs/capture-screenshots.sh. SVG keeps them crisp at any size and tiny.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="$ROOT/website/static/img"
FEAT="$OUT/features"
mkdir -p "$FEAT"

# card <path.svg> <w> <h> <title> <subtitle> <emoji-entity>
card() {
  local file="$1" w="$2" h="$3" title="$4" sub="$5" emoji="$6"
  cat > "$file" <<SVG
<svg width="$w" height="$h" viewBox="0 0 $w $h" xmlns="http://www.w3.org/2000/svg" role="img" aria-label="$title placeholder">
  <defs>
    <linearGradient id="bg" x1="0" y1="0" x2="$w" y2="$h" gradientUnits="userSpaceOnUse">
      <stop stop-color="#036FE2"/><stop offset="1" stop-color="#00C4B3"/>
    </linearGradient>
  </defs>
  <rect width="$w" height="$h" fill="url(#bg)"/>
  <path d="M0 $h L$((w/5)) $((h*46/100)) L$((w*2/5)) $((h*70/100)) L$((w*3/5)) $((h*34/100)) L$((w*4/5)) $((h*64/100)) L$w $((h*42/100)) L$w $h Z" fill="#ffffff" opacity="0.14"/>
  <text x="$((w/2))" y="$((h*47/100))" font-family="system-ui,Segoe UI,Roboto,sans-serif" font-size="$((h/9))" font-weight="800" fill="#ffffff" text-anchor="middle">$emoji  $title</text>
  <text x="$((w/2))" y="$((h*60/100))" font-family="system-ui,Segoe UI,Roboto,sans-serif" font-size="$((h/24))" fill="#eaf3ff" text-anchor="middle">$sub</text>
</svg>
SVG
  echo "  $file"
}

card "$FEAT/terrain-hero.svg" 1200 675 "3D Terrain" "placeholder — replace with a real capture" "&#9968;&#65039;"
card "$FEAT/contours.svg"     1200 675 "On-the-fly Contours" "placeholder — replace with a real capture" "&#12336;&#65039;"
card "$FEAT/hillshade.svg"    1200 675 "Hillshade" "placeholder — replace with a real capture" "&#127748;"
card "$OUT/social-card.svg"   1200 630 "CARTO Mobile SDK" "Maps, terrain &amp; routing for Android and iOS" "&#128506;&#65039;"
echo "done."
