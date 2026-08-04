#version 330

// Perspective grid running to a banded sun. One full screen quad, no texture
// reads, no noise: the floor is a single divide per pixel and the rest is
// gradients, so this is cheaper than the star field and much cheaper than
// anything built on fbm.

out vec4 finalColor;

uniform float time;
uniform vec2 resolution;  // framebuffer pixels, not logical points

const float HORIZON = 0.52;
const float SCALE = 4.0;   // grid cells across the floor
const float SPEED = 0.25;  // cells per second coming at the viewer

// Anti-aliased line wherever v crosses an integer. fwidth widens the line as
// the rows converge towards the horizon, which is what stops them tearing into
// a shimmering mess instead of blurring into haze.
float grid_line(float v) {
    float d = abs(fract(v + 0.5) - 0.5);
    return 1.0 - smoothstep(0.0, fwidth(v) * 1.2 + 0.010, d);
}

void main() {
    // See starfield.fs: fragTexCoord is unusable here, raylib gives shapes the
    // texture coordinates of one pixel in the font atlas.
    vec2 uv = gl_FragCoord.xy / resolution;
    uv.y = 1.0 - uv.y;

    float aspect = resolution.x / max(resolution.y, 1.0);

    // Sky, dark overhead warming into the haze the sun sits in.
    vec3 color = mix(vec3(0.05, 0.02, 0.13), vec3(0.34, 0.06, 0.40),
                     smoothstep(0.0, HORIZON, uv.y));
    color += vec3(0.50, 0.10, 0.28) * smoothstep(HORIZON - 0.26, HORIZON, uv.y) * 0.65;

    // Sun, half swallowed by the horizon. The bands across its lower half are
    // the whole look; a plain disc just reads as a sunset.
    vec2 sun = vec2((uv.x - 0.5) * aspect, uv.y - (HORIZON - 0.10));
    float sun_d = length(sun);
    float disc = smoothstep(0.195, 0.185, sun_d);

    // Gaps widen further down the disc, so the bands read as perspective
    // rather than as a fixed pattern laid over a circle.
    float band = mix(1.0, step(0.40 + sun.y * 1.2, fract(sun.y * 34.0)), step(-0.03, sun.y));

    vec3 sun_color = mix(vec3(1.00, 0.88, 0.35), vec3(1.00, 0.16, 0.55),
                         smoothstep(-0.19, 0.19, sun.y));

    color = mix(color, sun_color, disc * band);
    color += vec3(0.90, 0.20, 0.50) * smoothstep(0.42, 0.19, sun_d) * 0.30;

    // Floor. Clamped away from zero because the reciprocal is evaluated for
    // every pixel including the sky, and an infinity multiplied by the mask
    // below would come back as a NaN rather than nothing.
    float t = max(uv.y - HORIZON, 0.0015);
    float depth = 1.0 / t;

    float gx = (uv.x - 0.5) * aspect * depth * SCALE;
    float gz = (depth + time * SPEED) * SCALE;
    float lines = max(grid_line(gx), grid_line(gz));

    float near = smoothstep(0.0, 0.34, t);
    vec3 grid = mix(vec3(1.00, 0.20, 0.70), vec3(0.25, 0.95, 1.00), near);

    vec3 floor_color = vec3(0.05, 0.01, 0.11);
    floor_color += vec3(0.60, 0.10, 0.38) * smoothstep(0.26, 0.0, t) * 0.5;
    floor_color += grid * lines * smoothstep(0.0, 0.09, t);

    color = mix(color, floor_color, step(HORIZON, uv.y));

    // Glow along the horizon itself, hiding the seam where the two meet.
    color += vec3(1.00, 0.30, 0.65) * smoothstep(0.028, 0.0, abs(uv.y - HORIZON)) * 0.55;

    // Settle the edges so the playfield stays the brightest thing on screen.
    float vignette = smoothstep(1.30, 0.30, length(uv - 0.5) * 2.0);
    color *= 0.52 + 0.48 * vignette;

    finalColor = vec4(color, 1.0);
}
