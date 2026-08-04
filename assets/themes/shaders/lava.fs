#version 330

// Lava lamp. Five metaballs summed analytically, thresholded into a surface.
//
// No noise and no hashing: each blob costs one sine and one reciprocal, and
// the merging comes free out of the summation rather than out of sampling
// anything. That keeps it to six sines a pixel, which matters because trig is
// the expensive part on the old integrated hardware this has to run on.

out vec4 finalColor;

uniform float time;
uniform vec2 resolution;  // framebuffer pixels, not logical points

const int COUNT = 5;

// Everything below reads the slowed clock, so this is the one number to turn
// if the lamp wants to move faster or slower. At this rate a blob takes about
// half a minute to climb, which is roughly what a real one does.
const float SPEED = 0.28;

void main() {
    // See starfield.fs: fragTexCoord is unusable here, raylib gives shapes the
    // texture coordinates of one pixel in the font atlas.
    vec2 uv = gl_FragCoord.xy / resolution;
    uv.y = 1.0 - uv.y;

    float aspect = resolution.x / max(resolution.y, 1.0);
    vec2 p = vec2(uv.x * aspect, uv.y);

    float t = time * SPEED;

    // One shared breath rather than a sine per blob. Nobody can tell that they
    // swell in unison, and it saves four transcendentals a pixel.
    float pulse = 1.0 + 0.10 * sin(t * 0.4);

    float sum = 0.0;

    for (int i = 0; i < COUNT; i++) {
        float fi = float(i);

        // Ping pong up and down the lamp. A fract() alone would teleport the
        // blob back to the bottom every cycle; folding it about 1.0 turns the
        // sawtooth into a triangle so it sinks back down instead.
        float rise = abs(fract(0.19 * fi + t * (0.055 + 0.026 * fi)) * 2.0 - 1.0);
        float sway = 0.5 + 0.26 * sin(t * (0.19 + 0.06 * fi) + fi * 2.1);

        vec2 d = p - vec2(sway * aspect, rise);
        float r = (0.015 + 0.011 * fract(fi * 0.37)) * pulse;

        // The epsilon keeps the centre of a blob finite.
        sum += r / (dot(d, d) + 0.0004);
    }

    vec3 color = mix(vec3(0.055, 0.018, 0.10), vec3(0.15, 0.03, 0.17), uv.y);
    vec3 lava = mix(vec3(1.00, 0.38, 0.10), vec3(1.00, 0.12, 0.45), uv.y);

    // Glow first, then the body over it, then the rim where the surface turns
    // away. Three bands off one field value.
    color += lava * smoothstep(0.20, 0.90, sum) * 0.10;
    color = mix(color, lava, smoothstep(0.85, 1.35, sum) * 0.85);
    color += vec3(1.00, 0.60, 0.30) *
             (smoothstep(0.72, 0.96, sum) - smoothstep(0.96, 1.60, sum)) * 0.30;

    // Settle the edges so the playfield stays the brightest thing on screen.
    float vignette = smoothstep(1.30, 0.30, length(uv - 0.5) * 2.0);
    color *= 0.55 + 0.45 * vignette;

    finalColor = vec4(color, 1.0);
}
