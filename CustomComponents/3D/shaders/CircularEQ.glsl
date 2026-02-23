// === VERTEX ===
void main() {
    gl_Position = vec4(position.xy, 0.0, 1.0);
}

// === FRAGMENT ===
// CircularEQ  3D ring of cylinders + spheres driven by audio spectrum

#define PI      3.14159265359
#define TWO_PI  6.28318530718
#define N       72        // columns on the ring
#define RING_R  3.1       // ring radius
#define CYL_R   0.095     // cylinder radius
#define MAX_H   4.6       // max column height
#define SPH_MAX 0.20      // max sphere radius

vec3 aces(vec3 x){
    return clamp((x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14),0.0,1.0);
}

float spec(float f){
    return clamp(texture2D(iChannel0, vec2(clamp(f,0.001,0.999), 0.25)).r, 0.0, 1.0);
}

float colH(int i){
    float f = float(i) / float(N);
    return spec(f * 0.88 + 0.02) * MAX_H;
}
vec3 colP(int i){
    float a = float(i) / float(N) * TWO_PI;
    return vec3(RING_R * cos(a), 0.0, RING_R * sin(a));
}

// Capped cylinder base at y=0, top at y=h
float sdCyl(vec3 p, float h, float r){
    vec2 d = abs(vec2(length(p.xz) - r, p.y - h*0.5)) - vec2(0.0, h*0.5);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}
float sdSph(vec3 p, float r){ return length(p) - r; }

// Returns vec2(dist, encoded_matID)
// matID: -1 = floor, 0..N-1 = cylinder index, N..2N-1 = sphere index
vec2 map(vec3 p){
    float d = 1e9; float mi = -1.0;

    // Floor
    float df = p.y;
    if(df < d){ d = df; mi = -1.0; }

    // Nearest columns by angle
    float ang = atan(p.z, p.x);
    float fci = (ang / TWO_PI + 1.0) * float(N);

    for(int di = -3; di <= 3; di++){
        float cf = fci + float(di);
        int ci = int(mod(cf + float(N)*8.0, float(N)));

        vec3 cp = colP(ci);
        float h  = colH(ci);
        float sr = (h / MAX_H) * SPH_MAX + 0.025;

        // Cylinder
        float dc = sdCyl(p - cp, h, CYL_R);
        if(dc < d){ d = dc; mi = float(ci); }

        // Sphere (sits at base, center at y=sr)
        float ds = sdSph(p - (cp + vec3(0.0, sr, 0.0)), sr);
        if(ds < d){ d = ds; mi = float(ci + N); }
    }

    return vec2(d, mi);
}

vec3 calcN(vec3 p){
    vec2 e = vec2(0.0012, 0.0);
    return normalize(vec3(
        map(p+e.xyy).x - map(p-e.xyy).x,
        map(p+e.yxy).x - map(p-e.yxy).x,
        map(p+e.yyx).x - map(p-e.yyx).x
    ));
}

float softShadow(vec3 ro, vec3 rd, float tmin, float tmax, float k){
    float res = 1.0; float t = tmin;
    for(int i = 0; i < 20; i++){
        float h = map(ro + rd*t).x;
        res = min(res, k*h/t);
        t += clamp(h, 0.04, 0.4);
        if(res < 0.005 || t > tmax) break;
    }
    return clamp(res, 0.0, 1.0);
}

float calcAO(vec3 p, vec3 n){
    float ao = 0.0, sca = 1.0;
    for(int i = 0; i < 5; i++){
        float h = 0.01 + 0.14 * float(i) / 4.0;
        float d = map(p + h*n).x;
        ao += (h - d) * sca;
        sca *= 0.93;
    }
    return clamp(1.0 - 2.8 * ao, 0.0, 1.0);
}

vec3 pillarColor(int ci, float heightRatio){
    // bass (ci=0) = hot magenta, treble (ci=N) = cobalt blue
    float fi = float(ci) / float(N);
    vec3 mag = vec3(1.0,  0.06, 0.62);
    vec3 pur = vec3(0.62, 0.12, 0.95);
    vec3 blu = vec3(0.15, 0.22, 1.0);
    vec3 base;
    if(fi < 0.5) base = mix(mag, pur, fi * 2.0);
    else          base = mix(pur, blu, (fi - 0.5) * 2.0);
    return base;
}

void main(){
    vec2 res = iResolution.xy;
    vec2 uv  = (gl_FragCoord.xy / res) * 2.0 - 1.0;
    uv.x    *= res.x / res.y;

    // Camera slowly orbiting
    float camA = iTime * 0.10 + 0.6;
    vec3 ro = vec3(cos(camA)*8.2, 5.8, sin(camA)*8.2);
    vec3 ta = vec3(0.0, 1.8, 0.0);
    vec3 ww = normalize(ta - ro);
    vec3 uu = normalize(cross(vec3(0,1,0), ww));
    vec3 vv = cross(ww, uu);
    vec3 rd = normalize(uv.x*uu + uv.y*vv + 1.75*ww);

    // Background: soft gray gradient (top lighter)
    vec3 bgTop = vec3(0.93, 0.93, 0.96);
    vec3 bgBot = vec3(0.78, 0.78, 0.84);
    vec3 bgCol = mix(bgBot, bgTop, clamp(rd.y * 1.4 + 0.5, 0.0, 1.0));

    // Raymarch
    float t = 0.2; float mi = -2.0; bool hit = false;
    for(int i = 0; i < 90; i++){
        vec2 r2 = map(ro + rd*t);
        if(r2.x < 0.0018){ mi = r2.y; hit = true; break; }
        if(t > 45.0) break;
        t += r2.x;
    }

    vec3 col;
    if(!hit){
        col = bgCol;
    } else {
        vec3 p = ro + rd*t;
        vec3 n = calcN(p);

        vec3 lig = normalize(vec3(0.7, 1.6, 0.5));   // key
        vec3 lif = normalize(vec3(-1.0, 0.9, -0.4)); // fill

        float diff = max(dot(n, lig), 0.0);
        float fill = max(dot(n, lif), 0.0) * 0.22;
        float amb  = 0.20;

        float sha = softShadow(p + n*0.015, lig, 0.02, 18.0, 14.0);
        float ao  = calcAO(p, n);

        vec3 hal  = normalize(lig - rd);
        float spe = pow(max(dot(n, hal), 0.0), 55.0);

        vec3 matCol;

        int imi = int(mi);
        if(imi == -1){
            // Floor  almost white
            matCol = vec3(0.97, 0.97, 0.99);
            spe   *= 0.08;
        } else {
            int ci = imi >= N ? imi - N : imi;
            float h = colH(ci);
            vec3 base = pillarColor(ci, h / MAX_H);

            if(imi >= N){
                // Sphere  glossy
                matCol = base * 1.05;
                spe   *= 1.5;
            } else {
                // Cylinder  vertical stripe shading
                float stripe = sin(p.y * 30.0) * 0.5 + 0.5;
                matCol = mix(base * 0.72, base, stripe * 0.3 + 0.70);
                // Brighter toward top
                float ht = clamp(p.y / max(h, 0.01), 0.0, 1.0);
                matCol = mix(matCol * 0.75, matCol * 1.1, ht * 0.55);
                spe   *= (0.8 + 0.5 * ht);
            }
        }

        float light = (diff * sha + fill + amb) * ao;
        col = matCol * light + vec3(1.0) * spe * sha * 0.55;

        // Rim
        float rim = pow(1.0 - max(dot(n, -rd), 0.0), 4.0) * 0.12;
        col += matCol * rim;

        // Distance fog
        float fog = exp(-t * 0.038);
        col = mix(bgCol, col, fog);
    }

    col = aces(col * 1.08);
    gl_FragColor = vec4(col, 1.0);
}
