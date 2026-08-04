#version 330

// Slow drift through a star field. One full screen quad, no texture reads, so
// it costs a single draw call regardless of window size.
//
// The budget here matters: this runs on nine year old integrated graphics. The
// whole shader is around a dozen sin() calls per pixel, which is less than the
// three octave noise field it replaced, because the stars are placed by hashing
// a grid rather than by sampling noise.

out vec4 finalColor;

uniform float time;
uniform vec2 resolution;  // framebuffer pixels, not logical points

vec2 hash22(vec2 p) {
    p = vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)));
    return fract(sin(p) * 43758.5453);
}

float hash12(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

// Two octaves is all the nebula needs. It never has an edge in it, so more
// detail would only cost time nobody can see.
float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);

    return mix(
        mix(hash12(i), hash12(i + vec2(1.0, 0.0)), u.x),
        mix(hash12(i + vec2(0.0, 1.0)), hash12(i + vec2(1.0, 1.0)), u.x),
        u.y
    );
}

float clouds(vec2 p) {
    return noise(p) * 0.65 + noise(p * 2.11) * 0.35;
}

// One star per grid cell, drifting downward. Only the cell under the pixel is
// tested: keeping stars clear of the cell edges means a star is never cut in
// half, which saves checking the eight neighbouring cells for overlap.
float star_layer(vec2 uv, float density, float drift, float radius, float seed) {
    vec2 p = uv * density - vec2(0.0, time * drift) + seed;
    vec2 cell = floor(p);
    vec2 rnd = hash22(cell);

    float presence = step(0.86, fract(rnd.x + rnd.y * 1.7));
    float d = length(fract(p) - (rnd * 0.6 + 0.2));

    // Each star breathes on its own phase, taken from where it sits rather
    // than from another hash.
    float twinkle = 0.62 + 0.38 * sin(time * 0.9 + (rnd.x + rnd.y) * 6.283);

    return smoothstep(radius, 0.0, d) * presence * twinkle;
}

void main() {
    // Not fragTexCoord. raylib batches shapes through the font atlas, so a
    // rectangle's texture coordinates cover the one white pixel it borrows from
    // there: they span about 1/128, not 0 to 1. Anything built on a grid would
    // land entirely inside a single cell. gl_FragCoord counts real pixels and
    // owes nothing to how the quad was drawn.
    vec2 uv = gl_FragCoord.xy / resolution;
    uv.y = 1.0 - uv.y;  // gl_FragCoord counts up from the bottom

    // Square the coordinates so stars stay round and the field does not stretch
    // when the window is resized.
    vec2 sky = vec2(uv.x * resolution.x / max(resolution.y, 1.0), uv.y);

    vec3 color = mix(vec3(0.050, 0.040, 0.086), vec3(0.012, 0.014, 0.030), uv.y);

    float drift = time * 0.006;
    float cloud = clouds(sky * 1.7 + vec2(drift, -drift * 0.45));

    color += vec3(0.26, 0.12, 0.44) * smoothstep(0.48, 0.95, cloud) * 0.60;
    color += vec3(0.05, 0.24, 0.34) * smoothstep(0.46, 0.92, 1.0 - cloud) * 0.35;

    // Three layers at different speeds and sizes. The parallax is what sells
    // the motion; a single layer just looks like the image is sliding.
    color += vec3(0.55, 0.60, 0.78) * star_layer(sky, 34.0, 0.010, 0.070, 0.0) * 0.55;
    color += vec3(0.82, 0.86, 0.96) * star_layer(sky, 21.0, 0.024, 0.060, 3.7) * 0.85;
    color += vec3(1.00, 0.97, 0.90) * star_layer(sky, 12.0, 0.042, 0.045, 8.2);

    // Settle the edges so the playfield stays the brightest thing on screen.
    float vignette = smoothstep(1.30, 0.30, length(uv - 0.5) * 2.0);
    color *= 0.55 + 0.45 * vignette;

    finalColor = vec4(color, 1.0);
}
