# Puzzie

Minimal offline Tetris for Mac, built with C and [raylib](https://www.raylib.com/).

## Requirements

- macOS 13+
- Homebrew packages: `raylib`, `cmake`, `pkg-config`

```bash
brew install raylib cmake pkg-config
```

## Build and run

```bash
chmod +x build.sh
./build.sh
./build/puzzie
```

## Modes

| Mode | Rules |
|------|-------|
| **40 Lines** | Clear 40 lines as fast as possible at a fixed level 1 speed. Timed, with a top 10 table. |
| **Zen** | Pick a speed from 1 to 15. It never changes and there is no game over — topping out sweeps the board and play continues. |
| **Marathon** | Pick a starting level. Speed rises one level every 10 lines and the run ends on a top out. Top 10 scores are kept. |

## Rules

The core follows the Tetris Guideline:

- **Gravity** — seconds per row is `(0.8 - (level - 1) * 0.007) ^ (level - 1)`, the
  Tetris Worlds curve used by every guideline game. Level 1 is one second per row,
  level 15 is about 7 milliseconds.
- **Levels** — one per 10 lines. Starting at level 5 still requires 50 total lines
  to reach level 6.
- **Scoring** — 100 / 300 / 500 / 800 times level for a single through tetris,
  50 x combo x level for consecutive clears, 1.5x while chaining back-to-back
  tetrises, 1 point per soft dropped cell and 2 per hard dropped cell.
- **Piece order** — 7-bag randomizer, so every set of seven contains each piece once.

## Themes

Block art is data driven, declared in `assets/themes/themes.cfg`. A theme is a
**sheet**: one image holding an authored, fully coloured block per piece, laid
out on a grid.

```ini
[theme]
id = bevel
name = Beveled
sheet = bevel/blocks.png
```

The default grid is seven cells across in the order `I O T S Z J L`, so a
896 x 128 image needs no further configuration. Sheets in another layout
describe themselves with `columns`, `rows` and `order` rather than being
re-cut.

Nothing is recoloured at runtime, so what you draw is what appears on the
board, and the whole playfield batches into one draw call because every block
samples the same texture. A grayscale `tile` multiplied by the piece colour is
still supported as a fallback, and a theme with no art at all draws flat
rounded blocks in a palette you choose.

Themes also control the backdrop — a colour, a gradient, an image, or a
fragment shader — plus the fill, border, thickness, corner radius and optional
texture of the board well and the side panels.

Every key is optional and costs nothing when unused. Activating a theme folds
the whole manifest into plain values, so the draw path never checks whether a
feature is enabled; it draws what was already resolved. Only the active theme
holds textures or shaders.

Block art is always drawn axis aligned, never rotated with the piece, so
baked-in directional lighting stays consistent across the board.

`No Theme` is built into the binary and renders the plain colored squares. It is
always available even if the assets folder is missing.

Full specification and instructions for adding a theme:
[`assets/themes/README.md`](assets/themes/README.md).

## Fonts

The interface uses **SF Pro Rounded** for labels and **SF Mono** for numbers,
both loaded at runtime from `/System/Library/Fonts/`. Nothing is bundled, so no
Apple typeface is redistributed, and there is nothing to download.

If a face is missing the game walks a fallback list (SF Compact Rounded, SF Pro,
Arial Rounded Bold, then raylib's built-in font) and logs which one it picked.
Note that raylib's TTF parser cannot read `.ttc` collections, which rules out
most of the older macOS families such as Avenir Next and Helvetica Neue. To use
your own font, drop a `.ttf` in and add it to the candidate list at the top of
`src/ui.c`.

## Controls

| Input | Action |
|-------|--------|
| Mouse | All menus |
| Left / Right | Move piece (auto-repeats when held) |
| Up / X / W | Rotate clockwise |
| Z | Rotate counter-clockwise |
| Down | Soft drop |
| Space | Hard drop (locks instantly) |
| C / Shift | Hold piece |
| P | Pause |
| Enter | Retry after a run ends |
| Esc (hold) | Quit a run in progress, with an on-screen countdown ring |
| Esc | Back out of menus, or leave a finished run immediately |

Handling is tuned in `src/config.h`: auto-shift delay and repeat rate, soft drop
rate, and the lock delay a grounded piece gets before it commits.

## Saved data

Everything lives in `~/Library/Application Support/Puzzie/`:

| File | Contents |
|------|----------|
| `scores.txt` | Top 10 fastest 40 Lines times and top 10 Marathon scores |
| `settings.txt` | Selected theme |

## Project layout

```text
assets/
  themes/       Theme manifest, per-theme art and backdrop shaders
tools/
  gen_block_sheet.c  Generates the default block sheet, not part of the build
src/
  main.c        Entry point, scenes, input handling
  game.c        Board, pieces, modes, guideline rules
  render.c      Board, HUD and overlay drawing
  menu.c        Mouse-driven menu screens
  ui.c          Font loading, text and panel drawing helpers
  theme.c       Theme manifest parsing and texture management
  scores.c      High score persistence
  platform.c    Asset and app data path resolution
  config.h      Tunable constants
```
