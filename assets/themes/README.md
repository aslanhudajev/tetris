# Theme art specification

A theme is one grayscale block image plus a block of metadata in
[`themes.cfg`](themes.cfg). The game tints the image with the colour of
whichever piece it is drawing, so a single tile covers all seven pieces.

## Folder layout

```text
assets/
  themes/
    themes.cfg          manifest, declares every installed theme
    README.md           this file
    <theme-id>/
      tile.png          the block art
```

One folder per theme, named after its `id`. Paths in `themes.cfg` are relative
to `assets/themes/`, so a theme with `id = bevel` uses `tile = bevel/tile.png`.

## The tile image

| Property | Requirement |
|----------|-------------|
| Format | PNG, 8 bit RGBA |
| Size | 128 x 128, square, power of two |
| Colour | Grayscale — R, G and B equal in every pixel |
| Alpha | Shape mask. Transparent where the block should not cover the cell |
| Padding | Baked into the image. The tile is drawn edge to edge in its cell |

### Why grayscale

The tile is drawn with the piece colour as a multiply tint. White stays the
full piece colour, mid gray becomes a darker shade of it, black goes to black.
Build the art as if the piece were pure white and let the tint do the rest.

Keep the artwork bright. Because the tint multiplies, a tile whose body sits at
50% gray halves the brightness of every piece and the whole board looks muddy.
Aim for a body around 85% (roughly value 215) and use a narrow bevel range for
shading rather than a dark base. The example `bevel` tile runs from about 182 in
its deepest shadow to 254 at the lit corner.

### Why 128 x 128

Cells render around 30–40 points, which is 60–80 physical pixels on a Retina
Mac. 128 gives headroom for large windows without wasting memory. The game
generates mipmaps and filters trilinearly, so downscaling stays clean.

### Padding and gaps

The game draws the tile filling the entire cell with no inset. If you want a
gap between blocks, build it into the image as transparent margin. The example
`bevel` tile uses a 3 pixel transparent margin and a 16 pixel corner radius.

## The tile never rotates

The tile is drawn axis aligned at every cell, always at rotation zero,
regardless of which way the piece is turned. Baked-in directional lighting is
therefore safe: a bevel lit from the top left stays lit from the top left on
every block on the board.

This means the art must read correctly in isolation as a single square. Do not
draw anything that implies a direction relative to the piece, such as an arrow
or a shape that only makes sense joined to a neighbour.

## Landing preview

The ghost shows where the current piece will land. Themes choose how it draws:

```ini
ghost = tile       # faded copy of the block art (default)
ghost = outline    # hollow rectangle, like the untextured default
ghost_opacity = 0.13
```

`ghost_opacity` is optional and accepts 0 to 1. Left out, it defaults to `0.13`
for the tile style and `0.5` for the outline style. A theme with no `tile` always
draws outlines regardless of what it asks for.

## Colours

Every colour value is hex RGB, with or without a leading `#`.

Piece colours default to a bright, saturated palette chosen to survive the
multiply tint. Override any of them per theme to change the feel without
touching the art:

```ini
color_i = 22E2F5
color_o = FCD840
color_t = BA64FC
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

`background`, `panel` and `outline` control the frame around the board and the
hold and next boxes.

## Adding a theme

1. Create `assets/themes/<your-id>/` and put `tile.png` in it.
2. Append a `[theme]` block to `themes.cfg` with at least `id`, `name` and `tile`.
3. Restart the game and pick it from Themes in the main menu.

The selection is written to
`~/Library/Application Support/Puzzie/settings.txt` and restored on launch.
Only the active theme's texture is kept in memory.
