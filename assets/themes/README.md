# Theme specification

A theme is a block of keys in [`themes.cfg`](themes.cfg). Every key is
optional; the smallest useful theme is three lines:

```ini
[theme]
id = bevel
name = Beveled
sheet = bevel/blocks.png
```

Themes control block art, the backdrop, and the two surfaces the game draws
(the board well and the side panels). Jump to
[Blocks](#blocks) · [Backdrop](#backdrop) · [Panels and the well](#panels-and-the-well) ·
[Key reference](#key-reference).

## What "optional" costs

Nothing. When a theme is activated the manifest is folded into plain values —
colours, rectangles, mode enums — and the strings are never read again. The
draw path indexes precomputed arrays; it never asks *whether* a theme has a
feature, only draws what was resolved. Concretely:

- Sheet cell rectangles are computed once at load, not per block per frame.
- Block border colours are derived once, not re-darkened every frame.
- Exactly one backdrop mode runs per frame. Nothing is layered over anything.
- An image backdrop is one quad in every fit mode. Tiling uses repeat wrapping
  rather than a loop of draws.
- Only the active theme holds textures or shaders. Switching themes frees the
  old one first.

So a theme using every feature costs a handful more draw calls than a bare one,
and a theme using none costs exactly what the game did before any of this
existed.

## Blocks

Three ways to draw a block. Pick one; the game infers the mode from which key
you set.

| Mode | Key | What it does |
|------|-----|--------------|
| Sheet | `sheet` | Authored cell per piece, drawn as painted |
| Tinted | `tile` | One grey image multiplied by the piece colour |
| Solid | neither | Flat rounded rectangles in the piece colours |

### Solid blocks

Set no art at all and the game draws flat colour, which is fully stylable:

```ini
color_i = 00F0FF
color_o = FFE600
block_radius = 0.28        ; 0 is square, 0.5 is a circle
block_inset = 2            ; transparent gap around each block, in pixels
block_border_width = 2     ; 0 removes the border and its draw call
block_border = 102030      ; omit to derive it from the piece colour
```

`block_radius` is a fraction of the block rather than a pixel radius, so a
block keeps its shape whether it is drawn in the well or in a preview box a
third of the size.

## Why a sheet and not a tinted template

The obvious shortcut is one grayscale block multiplied by a per-piece colour.
It is still supported (see [Tinted tiles](#tinted-tiles)) but it has a ceiling
you will hit immediately, because a multiply tint can only scale brightness.
Every facet of the block ends up on the same hue, and the result is darker than
the colour you asked for.

Art that looks like a jewel gets there by shifting *hue* between facets: a red
block has orange highlights and a maroon shadow, not merely lighter and darker
red. No tint can invent that from grey. This is also why commercial puzzle
games ship per-colour texture maps rather than tinting a template.

Baking colour in has two other benefits. Every block on the board samples one
texture, so the whole playfield batches into a single draw call. And what you
see in your art tool is what you get in game, with no runtime maths in between.

## Folder layout

```text
assets/
  themes/
    themes.cfg          manifest, declares every installed theme
    README.md           this file
    <theme-id>/
      blocks.png        the sheet
```

One folder per theme, named after its `id`. Paths in `themes.cfg` are relative
to `assets/themes/`, so a theme with `id = bevel` uses `sheet = bevel/blocks.png`.

## The sheet

| Property | Requirement |
|----------|-------------|
| Format | PNG, 8 bit RGBA |
| Layout | A grid of equally sized cells |
| Cell size | Image size divided by the grid. 128 x 128 recommended |
| Alpha | Shape mask. Transparent where the block should not cover the cell |
| Padding | Baked in. A cell is drawn edge to edge in its board cell |

The default grid is seven cells across and one down, in the order
`I O T S Z J L`. A 896 x 128 image therefore needs no configuration at all.

### Describing a different layout

Ripped sheets are rarely in the order this game wants and often carry extra
cells. Describe the layout rather than re-cutting the image:

```ini
sheet = ripped/blocks.png
columns = 8
rows = 2
order = S Z J L T O I - - - - - - - - -
```

`order` is a single line covering the whole grid, left to right then top to
bottom. Each letter names the piece that cell holds; `-` skips a cell. Above,
the first seven cells are pieces, the eighth and all of row two are ignored.
Skipped cells cost nothing but space in the image.

On load the game logs the grid it worked out:

```text
THEME: 'bevel' sheet 896x128, 7x1 grid, cells of 128x128
```

It warns if the image does not divide evenly into the grid, or if `order` points
at a cell outside it. If blocks come out sliced wrong, that line tells you
whether the game and your art tool disagree about the layout.

### Seams

Cells are sampled with half a texel of inset and no mipmaps, so neighbouring
cells cannot bleed into each other. Leaving a pixel or two of transparent gutter
around each block is still good practice.

### Why 128 x 128 cells

Board cells render around 30–40 points, which is 60–80 physical pixels on a
Retina Mac. 128 leaves headroom for large windows without wasting memory.

### Blocks never rotate

A cell is drawn axis aligned at rotation zero regardless of which way the piece
is turned. Baked-in directional lighting is therefore safe: a block lit from the
top left stays lit from the top left everywhere on the board.

This means each block must read correctly in isolation as a single square. Do
not draw anything implying a direction relative to the piece, such as an arrow
or a shape that only makes sense joined to a neighbour.

### Generating a sheet

[`tools/gen_block_sheet.c`](../../tools/gen_block_sheet.c) builds the default
`bevel` sheet procedurally and is a working reference for hue-shifted shading.
It is not part of the game build:

```sh
clang -std=c11 tools/gen_block_sheet.c -o /tmp/gen $(pkg-config --cflags --libs raylib) -lm
/tmp/gen assets/themes/bevel/blocks.png
```

## Tinted tiles

The fallback path. One grayscale image, multiplied by the piece colour:

```ini
tile = bevel/tile.png
```

Useful for quick tests and for themes you want to recolour from the manifest
without touching art. Keep the artwork bright, because the tint multiplies: a
tile whose body sits at 50% grey halves the brightness of every piece. Aim for a
body around 85% and use a narrow range for shading rather than a dark base.

`sheet` wins if a theme sets both.

## Backdrop

Four modes, and exactly one runs per frame.

```ini
background = 14141C            ; solid, plus a derived gradient by default
background_bottom = 000000     ; explicit gradient stop
background_flat                ; solid, no gradient
background_image = art/bg.png  ; with background_fit
background_shader = shaders/aurora.fs
```

`background_fit` accepts `stretch`, `cover` (preserve aspect, crop the
overflow) or `tile` (repeat). All three are a single quad — tiling uses texture
repeat wrapping rather than a loop of draws.

### Backdrop shaders

A fragment shader, GLSL `#version 330`, using raylib's default vertex stage. It
receives two uniforms beyond the raylib built-ins:

| Uniform | Type | Value |
|---------|------|-------|
| `time` | `float` | Seconds since launch |
| `resolution` | `vec2` | Window size in pixels |

`fragTexCoord` runs 0 to 1 across the window.
[`shaders/aurora.fs`](shaders/aurora.fs) is a working example. If a shader
fails to compile the game logs the error and falls back to the gradient, so a
broken shader never leaves you with a blank window.

This is the one place a shader is genuinely free: it is a single full-screen
quad, so it costs one draw call no matter how elaborate the effect. Panels are
deliberately not shader-capable — they are many small rectangles, each of which
would force its own batch flush, and rounded rectangles do not carry usable
local UVs for a shader to work with.

## Panels and the well

Two surfaces, same six keys each, prefixed `panel_` or `well_`. Panels are the
hold and next boxes; the well is the frame around the board.

```ini
panel = 22222C               ; fill, accepts RGBA for translucency
panel_border = 3C3C4A
panel_border_width = 1.5     ; 0 removes the border and its draw call
panel_radius = 8             ; corner radius in pixels
panel_image = art/panel.png  ; replaces the fill
panel_fit = stretch
```

Leave `well_border` out and the frame is tinted with the current mode's accent
colour — cyan for 40 Lines, violet for Zen, amber for Marathon. Setting it
takes that over.

Fills accept eight-digit RGBA, which is how the `aurora` theme lets its
backdrop show through the panels.

## Landing preview

The ghost shows where the current piece will land:

```ini
ghost = tile       # faded copy of the block art (default)
ghost = outline    # hollow rectangle
ghost_opacity = 0.13
```

`ghost_opacity` accepts 0 to 1. Left out, it defaults to `0.13` for the tile
style and `0.5` for the outline style. A theme with no art always draws outlines
regardless of what it asks for.

## Colours

Every colour value is hex RGB, with or without a leading `#`.

`background`, `panel` and `outline` control the frame around the board and the
hold and next boxes.

Piece colours are used for flat blocks when a theme has no art, for the ghost
outline, and for the swatches in the Themes menu. Sheet art ignores them,
because that art is drawn exactly as authored.

```ini
color_i = 22E2F5
color_o = FCD840
```

| Piece | Default |
|-------|---------|
| I | `22E2F5` |
| O | `FCD840` |
| T | `BA64FC` |
| S | `4CE068` |
| Z | `FC5864` |
| J | `508CFC` |
| L | `FC9C38` |

## Key reference

| Key | Default | Notes |
|-----|---------|-------|
| `id` `name` `flavor` `author` | — | `id` is the folder name and the saved selection |
| `sheet` | — | Authored art, one cell per piece |
| `columns` `rows` | `7` `1` | Sheet grid |
| `order` | `I O T S Z J L` | Piece per cell, `-` skips |
| `tile` | — | Grayscale art, tinted per piece |
| `color_i` … `color_l` | bright palette | Palette, and the fill for solid blocks |
| `block_radius` | `0.18` | Fraction of the block, 0 to 0.5 |
| `block_inset` | `1` | Gap around each block, pixels |
| `block_border_width` | `1` | 0 removes the border |
| `block_border` | derived | Solid blocks only |
| `background` | `121218` | Base colour |
| `background_bottom` | derived | Gradient stop |
| `background_flat` | — | Solid, no gradient |
| `background_image` | — | With `background_fit` |
| `background_shader` | — | Fragment shader |
| `background_fit` | `cover` | `stretch` `cover` `tile` |
| `panel` `well` | dark greys | Fill, RGB or RGBA |
| `panel_border` `well_border` | grey / mode accent | Edge colour |
| `panel_border_width` `well_border_width` | `1.5` `2` | 0 removes the edge |
| `panel_radius` `well_radius` | `8` `10` | Pixels |
| `panel_image` `well_image` | — | With `panel_fit` / `well_fit` |
| `ghost` | `tile` | `tile` or `outline` |
| `ghost_opacity` | `0.13` / `0.5` | 0 to 1 |

Unknown keys are logged as warnings rather than ignored silently, so a typo
shows up the first time you run the game.

## Adding a theme

1. Create `assets/themes/<your-id>/` and put your art in it.
2. Append a `[theme]` block to `themes.cfg` with at least `id` and `name`.
3. Restart the game and pick it from Themes in the main menu.

The selection is written to
`~/Library/Application Support/Puzzie/settings.txt` and restored on launch.
Only the active theme's texture is kept in memory.
