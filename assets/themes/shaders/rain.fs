#version 330

// Falling columns of light. The screen is a grid of cells; each column has one
// bright head running down it with a fading tail behind, and every cell flickers
// on its own so the trail reads as characters rather than as a gradient.
//
// Three sines a pixel: two to give a column its speed and phase, one for the
// flicker. Everything else is arithmetic on the cell index, so the cost does not
// change with how much of the screen is lit.

out vec4 finalColor;

uniform float time;
uniform vec2 resolution;  // framebuffer pixels, not logical points

const float ROWS = 34.0;

// How fast the columns fall. The slowest now takes about fifteen seconds to
// cross the screen, the fastest about five.
const float SPEED = 0.30;

// How often a lit cell picks a new brightness, in changes a second. Kept off
// the fall speed on purpose: slowed to match, the trails stop reading as
// characters turning over and settle into plain stripes.
const float CHURN = 3.0;
const vec3 TRAIL = vec3(0.20, 1.00, 0.42);
const vec3 HEAD = vec3(0.80, 1.00, 0.88);

float hash11(float n) {
    return fract(sin(n * 127.1) * 43758.5453);
}

vec2 hash21(float n) {
    return fract(sin(vec2(n * 127.1, n * 311.7)) * 43758.5453);
}

void main() {
    // See starfield.fs: fragTexCoord is unusable here, raylib gives shapes the
    // texture coordinates of one pixel in the font atlas.
    vec2 uv = gl_FragCoord.xy / resolution;
    uv.y = 1.0 - uv.y;

    float aspect = resolution.x / max(resolution.y, 1.0);

    // Round the column count so cells stay square whatever shape the window is.
    float columns = max(floor(ROWS * aspect + 0.5), 1.0);

    vec2 grid = vec2(uv.x * columns, uv.y * ROWS);
    vec2 cell = floor(grid);
    vec2 within = fract(grid);

    // Speed and starting offset per column. The run is longer than the screen
    // at both ends so a column is never seen to begin or to stop.
    vec2 rnd = hash21(cell.x + 1.0);
    float speed = (0.22 + 0.50 * rnd.x) * SPEED;
    float head = fract(rnd.y + time * speed) * (ROWS + 20.0) - 10.0;

    // Distance behind the head, in cells. Ahead of it is unlit.
    float behind = head - cell.y;
    float tail = 7.0 + 9.0 * rnd.x;
    float fade = clamp(1.0 - behind / tail, 0.0, 1.0) * step(0.0, behind);

    // Each cell changes several times a second, which is what sells the trail
    // as glyphs turning over rather than as a smear.
    float flicker = 0.55 + 0.45 * hash11(cell.x * 31.7 + cell.y * 7.3 + floor(time * CHURN));

    // Gaps around each cell so the column reads as separate marks.
    vec2 mark = step(vec2(0.16), within) * step(within, vec2(0.84));
    float lit = mark.x * mark.y;

    vec3 color = vec3(0.010, 0.020, 0.014);
    color += TRAIL * fade * fade * flicker * lit * 0.85;
    color += HEAD * smoothstep(1.4, 0.0, abs(behind)) * lit;

    // Settle the edges so the playfield stays the brightest thing on screen.
    float vignette = smoothstep(1.30, 0.30, length(uv - 0.5) * 2.0);
    color *= 0.50 + 0.50 * vignette;

    finalColor = vec4(color, 1.0);
}
