#version 330

// Slow drifting colour field. One full screen quad, no texture reads, so it
// costs a single draw call regardless of window size.

in vec2 fragTexCoord;
out vec4 finalColor;

uniform float time;
uniform vec2 resolution;

// Cheap value noise. Three octaves is plenty for something this soft.
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);

    return mix(
        mix(hash(i), hash(i + vec2(1.0, 0.0)), u.x),
        mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), u.x),
        u.y
    );
}

float fbm(vec2 p) {
    return noise(p) * 0.5 + noise(p * 2.03) * 0.25 + noise(p * 4.01) * 0.125;
}

void main() {
    vec2 uv = fragTexCoord;
    uv.x *= resolution.x / max(resolution.y, 1.0);

    float drift = time * 0.02;
    float field = fbm(uv * 2.2 + vec2(drift, drift * 0.6));
    field += fbm(uv * 1.1 - vec2(drift * 0.8, drift * 0.3)) * 0.5;

    vec3 deep = vec3(0.06, 0.05, 0.11);
    vec3 violet = vec3(0.30, 0.17, 0.52);
    vec3 teal = vec3(0.10, 0.36, 0.46);

    vec3 color = mix(deep, violet, smoothstep(0.15, 0.85, field));
    color = mix(color, teal, smoothstep(0.45, 1.0, field) * 0.7);

    // Settle the edges so the playfield stays the brightest thing on screen.
    float vignette = smoothstep(1.35, 0.35, length(fragTexCoord - 0.5));
    color *= 0.5 + 0.5 * vignette;

    finalColor = vec4(color, 1.0);
}
