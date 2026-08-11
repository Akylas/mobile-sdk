# Style parameters (`nuti::`)

A style parameter is a value the **app** owns and the style reads. It is declared in the style
project's `nutiparameters` block and set at runtime through `MBVectorTileDecoder`:

```java
decoder.setStyleParameter("show_relief", "true");
decoder.setStyleParameters(Map.of("lang", "fr", "buildings", "1"));
decoder.setJSONStyleParameters("{\"lang\":\"fr\"}");
```

## Declaring one

```json
"nutiparameters": {
  "show_relief":  { "default": true },
  "lang":         { "default": "en" },
  "routes_type":  [0, 1, 2],
  "poi_colors":   { "default": { "restaurant": "#c0392b", "cafe": "#8e6e53" } },
  "zoom_steps":   { "default": [10, 12, 14] }
}
```

| form | meaning |
|---|---|
| `{ "default": <scalar> }` | a bool / integer / float / string parameter |
| `[a, b, c]` | an **enum**: the allowed values, the last one is the default |
| `{ "default": <object> }` / `{ "default": <array> }` | a **table** the style indexes into |

The array form has always meant "enum", so a table must be declared under `default`.

## Reading one

Scalars read like any other variable, in an expression or a filter:

```css
#road['nuti::show_underground' = 1] { line-color: @underground_color; }
#label { text-size: 12 / [nuti::_fontscale]; }
```

Tables are read with `get`, which takes an optional fallback, plus `has` and `length`:

```css
#poi { marker-fill: get([nuti::poi_colors], [class], #888888); }
#contour { line-width: get([nuti::widths], 0, 0.8); }
```

`get(table, key)` takes a member by name from an object, or an element by index from an array, and
is unset when the key is missing (so the third argument is what you usually want). One table
parameter replaces one parameter per class, and the app can rewrite the whole table at once:

```java
decoder.setStyleParameter("poi_colors", "{\"restaurant\":\"#c0392b\",\"cafe\":\"#8e6e53\"}");
```

`getStyleParameter` returns a table as JSON.

## What a change costs

Changing a parameter is either a **redraw** or a **re-decode of every visible tile** (~130 ms of CPU
per tile), decided per parameter when the style loads:

- **Redraw** — the parameter is read *only* by properties the renderer evaluates per frame:
  colours, opacities, widths, and only where the expression reads nothing else that is fixed at
  decode time.
- **Redraw** — or it SELECTS a feature (below).
- **Re-decode** — everything else, and deliberately so:
  - the parameter appears in a **filter** (`#road['nuti::x' = 1]`, `when (...)`): it decides which
    rules match, i.e. what geometry the tile contains at all;
  - it feeds a property that is *also* read while the tile is built — `text-size` and `shield-size`
    pick the glyph raster, marker `width`/`height`/`stroke-width` draw the generated bitmap, line
    `stroke-width` and `stroke-miterlimit` shape the stroke pattern;
  - the expression also reads a **feature field** or the zoom — `get([nuti::poi_colors], [class])`
    is in this group, because the class comes from the feature;
  - it is `_geometryscale`, `_fontscale` or `_zoomlevelbias`, which scale the geometry and the
    glyphs a tile is built with.

So a colour an app exposes as a setting (`"water_color"`) is free to change, while a table read per
feature class still costs a decode.

## Selecting one feature

The one "parameter compared with a feature field" that can be free is a SELECTION, and the style has
to **ask for it** — nothing is inferred, and a style that declares nothing is not even inspected:

```json
"nutiparameters": {
  "selected_id": { "default": "", "selects": true }
}
```

```css
@is_selected: [nuti::selected_id] = [osmid] + '';
#routes {
  line-color: @is_selected ? #ff3b00 : #3388ff;
  line-width: 5 + (@is_selected ? 4 : 0);
}
```

```java
decoder.setStyleParameter("selected_id", Long.toString(osmid));   // no tile is decoded again
```

The decoder folds the comparison both ways at decode, so the tile carries the selected and the
unselected appearance as two style slots, and each feature keeps a hash of what it is compared with.
Setting the parameter rewrites one byte per vertex and redraws.

The rules are narrow, and a style that breaks one falls back to the re-decode path — with a warning
in the log naming the reason, so `selects` never fails silently:

- only `line-color`, `line-opacity` and `line-width` of a **line** rule may read the parameter;
- always as `[nuti::x] = <expression of feature fields>`, with the same expression everywhere, and
  never together with another parameter in one property;
- never in a **filter** — `when ([nuti::selected_id] = [osmid] + '')::casing` decides whether the
  casing geometry EXISTS, and no repaint can build geometry. Write the casing as a width and a
  colour instead of as a rule if you want the selection to stay free;
- not on a **dashed** line whose width is selected: the dash raster is sized by the width.

See [`rendering/10-performance.md`](rendering/10-performance.md#selection-the-appearance-half-without-a-decode)
for the mechanism and the measurements.
