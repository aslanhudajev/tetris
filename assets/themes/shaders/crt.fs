#version 330

// Phosphor tube: barrel distorted glass, scanlines, a slow roll bar and a
// centre glow. No noise and no hashing, so this is the cheapest shader here.
//
// Swap PHOSPHOR for vec3(1.00, 0.72, 0.24) to get an amber terminal instead.

out vec4 finalColor;

uniform float time;
uniform vec2 resolution;  // framebuffer pixels, not logical points

const vec3 PHOSPHOR = vec3(0.36, 1.00, 0.55);
const float CURVE = 0.22;    // how far the glass bulges
const float SCANLINES = 220.0;

void main() {
    // See starfield.fs: fragTexCoord is unusable here, raylib gives shapes the
    // texture coordinates of one pixel in the font atlas.
    vec2 uv = gl_FragCoord.xy / resolution;
    uv.y = 1.0 - uv.y;

    // Bulge the picture outwards from the centre. Everything below samples the
    // warped coordinate, so the scanlines bow with the glass rather than
    // staying flat across a curved screen.
    vec2 centred = uv - 0.5;
    vec2 warped = uv + centred * dot(centred, centred) * CURVE;

    // Off the tube entirely: this is the bezel.
    vec2 edge = step(vec2(0.0), warped) * step(warped, vec2(1.0));
    float on_screen = edge.x * edge.y;

    // Phosphor glow, strongest in the middle where the beam spends its time.
    float falloff = 1.0 - dot(centred, centred) * 1.5;
    vec3 color = PHOSPHOR * 0.055 * max(falloff, 0.0);
    color += PHOSPHOR * 0.030;

    // Scanlines. The period is a fraction of the screen rather than a pixel
    // count, so it stays put across window sizes and does not beat against the
    // pixel grid on a Retina display.
    float scan = 0.5 + 0.5 * cos(warped.y * SCANLINES * 6.28318);
    color *= 1.0 - 0.42 * scan;

    // The roll bar drifting down the tube, the one artefact that makes a CRT
    // read as switched on rather than as a green gradient.
    float roll = fract(warped.y * 0.8 - time * 0.07);
    color += PHOSPHOR * smoothstep(0.10, 0.0, roll) * 0.045;

    // Mains hum, just enough to keep the brightness from sitting perfectly
    // still. Two cycles a second, under two percent.
    color *= 1.0 + 0.016 * sin(time * 12.0);

    color *= on_screen;

    // Vignette inside the glass, so the corners fall away before the bezel.
    float vignette = smoothstep(1.35, 0.35, length(centred) * 2.0);
    color *= 0.45 + 0.55 * vignette;

    finalColor = vec4(color, 1.0);
}
