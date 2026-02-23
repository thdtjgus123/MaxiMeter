// === VERTEX ===
void main() {
    gl_Position = vec4(position.xy, 0.0, 1.0);
}

// === FRAGMENT ===
// WaveformRibbon  Rainbow 3D audio spectrum ribbons

#define PI  3.14159265359
#define NUM_RIBBONS 8

vec3 aces(vec3 x){
    return clamp((x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14),0.0,1.0);
}

vec3 hsv(float h, float s, float v){
    vec3 c = abs(fract(vec3(h)+vec3(0.0,2.0,1.0)/3.0)*6.0-3.0);
    return v*mix(vec3(1.0),clamp(c-1.0,0.0,1.0),s);
}

// Sample FFT spectrum (row 0.25 = frequency bands)
float spec(float f){
    return clamp(texture2D(iChannel0, vec2(f, 0.25)).r, 0.0, 1.0);
}

// Sample waveform (row 0.75 = time-domain)
float wave(float x){
    return texture2D(iChannel0, vec2(clamp(x,0.001,0.999), 0.75)).r * 2.0 - 1.0;
}

// 3D ribbon: project a ribbon at world-Y=rbY, Z=rbZ into screen uv
// Returns (glow, lines) brightness for that ribbon
vec2 ribbonHit(vec2 uv, float rbZ, float rbY, float fi) {
    // Perspective divide: x shrinks with depth
    float depth = rbZ + 8.0;
    float scale = 5.5 / depth;

    // Screen X from world X (ribbon spans -PI..PI in world)
    // Solve world X from screen X: worldX = uv.x / scale
    float wx = uv.x / scale;
    if(abs(wx) > PI) return vec2(0.0);

    // Normalize to 0..1 for audio sample
    float sampleX = wx / (PI) * 0.5 + 0.5;

    // Waveform Y at this X position
    float wY = wave(sampleX);

    // Audio-reactive amplitude for this ribbon
    float freqBand = fi * 0.7 + 0.05;
    float amp = spec(freqBand);
    float amplitude = 0.9 + amp * 1.8;

    // World Y of the ribbon surface at this X
    float surfaceY = rbY + wY * amplitude;

    // Project surface Y to screen Y
    float scrY = surfaceY * scale;

    // Distance from uv.y to ribbon screen Y
    float dist = abs(uv.y - scrY);

    // Ribbon glow profile (two layers: sharp edge + soft glow)
    float g = exp(-dist * 80.0 * scale) * 1.2
            + exp(-dist * 30.0 * scale) * 0.5
            + exp(-dist * 10.0 * scale) * 0.2;

    // Vertical mesh lines (world-X grid)
    float gridX = abs(sin(wx * 28.0));
    float gridLine = pow(gridX, 120.0);

    // Also thin horizontal edge lines at the waveform surface
    float edgeMask = exp(-dist * 120.0 * scale);

    float brightness = g * (0.18 + gridLine * 0.82 + edgeMask * 0.6);
    brightness *= (0.4 + amp * 1.4);

    return vec2(brightness, amp);
}

void main(){
    vec2 res  = iResolution.xy;
    vec2 fc   = gl_FragCoord.xy;
    vec2 uv   = (fc / res) * 2.0 - 1.0;
    uv.x     *= res.x / res.y;

    // Global bass
    float bass = spec(0.04);

    vec3 col = vec3(0.0);

    // Ribbons are arranged in a fan: Z spread + slight Y separation
    for(int i = 0; i < NUM_RIBBONS; i++){
        float fi = float(i) / float(NUM_RIBBONS - 1);   // 0..1

        // Color: left=orange(0.07), through magenta(0.85), cyan(0.52), yellow(0.16)
        float hue = mod(0.07 + fi * 0.85, 1.0);

        // Z position: ribbons fan from near to far
        float rbZ = fi * 14.0 - 1.0;

        // Y position: ribbons cluster near center with slight offset
        float rbY = (fi - 0.5) * 0.15;

        vec2 hit = ribbonHit(uv, rbZ, rbY, fi);
        float brightness = hit.x;
        float amp = hit.y;

        // Saturation boost on loud ribbons
        vec3 rColor = hsv(hue, 0.85 + amp * 0.15, 1.0);

        // Additive blending with depth fade (far ribbons dimmer)
        float depthFade = 1.0 - fi * 0.55;
        col += rColor * brightness * depthFade;
    }

    // Subtle center horizontal bloom
    float centerGlow = exp(-abs(uv.y) * 6.0) * 0.04 * bass;
    col += vec3(0.5, 0.3, 0.8) * centerGlow;

    // Vignette
    float vign = 1.0 - smoothstep(0.9, 1.8, length(uv));
    col *= vign;

    col = aces(col * 1.4);
    gl_FragColor = vec4(col, 1.0);
}
