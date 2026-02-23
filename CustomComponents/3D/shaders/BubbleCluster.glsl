// === VERTEX ===
void main() {
    gl_Position = vec4(position.xy, 0.0, 1.0);
}

// === FRAGMENT ===
// BubbleCluster  two audio-reactive sphere clusters with flying particles

#define PI     3.14159265359
#define TWO_PI 6.28318530718
#define NS     18    // total spheres (9 per side)
#define NP     110   // particles

float hash(float n){ return fract(sin(n)*43758.5453123); }
vec3  hash3(float n){ return vec3(hash(n),hash(n+5.71),hash(n+11.3)); }

vec3 aces(vec3 x){
    return clamp((x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14),0.0,1.0);
}
float spec(float f){
    return clamp(texture2D(iChannel0,vec2(clamp(f,0.001,0.999),0.25)).r,0.0,1.0);
}

float smin(float a,float b,float k){
    float h=clamp(0.5+0.5*(b-a)/k,0.0,1.0);
    return mix(b,a,h)-k*h*(1.0-h);
}

// Returns vec2(dist, colorBlend)  blend 0=green 1=salmon
vec2 mapBlob(vec3 p){
    float d = 1e9;
    float bSum = 0.0, wSum = 0.0;
    for(int i=0; i<NS; i++){
        float fi  = float(i);
        float sideN= float(NS)/2.0;
        bool  left= fi < sideN;
        float fi2 = left ? fi : fi - sideN;

        vec3 h3 = hash3(fi*6.17+2.3);
        float theta = h3.x * PI;
        float phi   = h3.y * TWO_PI;
        float rr    = 0.42 + h3.z * 1.05;

        vec3 ofs = vec3(
            rr * sin(theta)*cos(phi) * 1.15,
            rr * cos(theta)          * 0.72,
            rr * sin(theta)*sin(phi) * 0.80
        );
        float cx = left ? -1.65 : 1.65;
        vec3 center = vec3(cx, 0.0, 0.0) + ofs;

        // Audio: left cluster  bass bands, right  treble bands
        float fBand = left ? (fi2/sideN*0.30+0.02)
                           : (fi2/sideN*0.30+0.52);
        float amp  = spec(fBand);
        float rad  = 0.40 + amp*0.30 + h3.z*0.12;

        float di = length(p - center) - rad;

        float bv = left ? 0.0 : 1.0;
        float w  = exp(-max(di,0.0)*2.5);
        bSum += bv*w; wSum += w;

        d = smin(d, di, 0.38);
    }
    float blend = wSum > 0.001 ? bSum/wSum : 0.0;
    return vec2(d, blend);
}

vec3 calcN(vec3 p){
    vec2 e=vec2(0.0014,0.0);
    return normalize(vec3(
        mapBlob(p+e.xyy).x-mapBlob(p-e.xyy).x,
        mapBlob(p+e.yxy).x-mapBlob(p-e.yxy).x,
        mapBlob(p+e.yyx).x-mapBlob(p-e.yyx).x
    ));
}

float softShadow(vec3 ro,vec3 rd,float tmin,float tmax,float k){
    float res=1.0; float t=tmin;
    for(int i=0;i<16;i++){
        float h=mapBlob(ro+rd*t).x;
        res=min(res,k*h/t);
        t+=clamp(h,0.05,0.5);
        if(res<0.005||t>tmax) break;
    }
    return clamp(res,0.0,1.0);
}

float calcAO(vec3 p,vec3 n){
    float ao=0.0,sca=1.0;
    for(int i=0;i<5;i++){
        float h=0.01+0.15*float(i)/4.0;
        ao+=(h-mapBlob(p+h*n).x)*sca;
        sca*=0.92;
    }
    return clamp(1.0-2.5*ao,0.0,1.0);
}

// Project 3D world point to UV screen coords (same basis as camera)
vec2 toScreen(vec3 wp,vec3 ro,vec3 uu,vec3 vv,vec3 ww,float fov){
    vec3 d=wp-ro;
    float cz=dot(d,ww);
    if(cz<0.1) return vec2(-9.0);
    float cx=dot(d,uu);
    float cy=dot(d,vv);
    return vec2(cx,cy)/cz*fov;
}

void main(){
    vec2 res = iResolution.xy;
    vec2 uv  = (gl_FragCoord.xy/res)*2.0-1.0;
    uv.x    *= res.x/res.y;

    // Camera
    float camA = iTime*0.12;
    vec3 ro = vec3(cos(camA)*8.4, 2.2, sin(camA)*8.4);
    vec3 ta = vec3(0.0, 0.2, 0.0);
    vec3 ww = normalize(ta-ro);
    vec3 uu = normalize(cross(vec3(0,1,0),ww));
    vec3 vv = cross(ww,uu);
    float fov = 1.72;
    vec3 rd  = normalize(uv.x*uu + uv.y*vv + fov*ww);

    vec3 bgCol = vec3(1.0);
    vec3 col   = bgCol;

    // Raymarch blob
    float t=0.3; bool hit=false; float blend=0.0;
    for(int i=0;i<88;i++){
        vec2 r2 = mapBlob(ro+rd*t);
        if(r2.x<0.0016){ hit=true; blend=r2.y; break; }
        if(t>40.0) break;
        t+=r2.x;
    }

    if(hit){
        vec3 p = ro+rd*t;
        vec3 n = calcN(p);

        vec3 lig = normalize(vec3(0.6,1.5,0.4));
        vec3 lif = normalize(vec3(-1.0,0.6,-0.3));

        float diff  = max(dot(n,lig),0.0);
        float fill  = max(dot(n,lif),0.0)*0.20;
        float amb   = 0.28;
        float sha   = softShadow(p+n*0.02,lig,0.05,15.0,12.0);
        float ao    = calcAO(p,n);
        vec3  hal   = normalize(lig-rd);
        float spe   = pow(max(dot(n,hal),0.0),60.0);

        // color: green side  salmon side, blend in center
        vec3 green  = vec3(0.47, 0.87, 0.38);
        vec3 center_= vec3(0.80, 0.78, 0.38);  // yellowish center merge
        vec3 salmon = vec3(0.97, 0.52, 0.44);

        vec3 matCol;
        if(blend < 0.5)
            matCol = mix(green,  center_, blend*2.0);
        else
            matCol = mix(center_, salmon, (blend-0.5)*2.0);

        float light = (diff*sha + fill + amb)*ao;
        col = matCol*light + vec3(1.0)*spe*sha*0.55;

        // Rim
        float rim = pow(1.0-max(dot(n,-rd),0.0),4.0)*0.10;
        col += matCol*rim;

        // Subtle subsurface
        float sss = exp(-t*0.06)*0.12;
        col += matCol*sss;

        // Fog
        col = mix(bgCol, col, exp(-t*0.022));
    }

    // ---- Particles overlay ----
    for(int i=0; i<NP; i++){
        float fi = float(i);
        vec3  h3 = hash3(fi*3.77+1.0);

        // Initial world position: distributed in an ellipsoid around cluster
        float theta = h3.x*PI;
        float phi   = h3.y*TWO_PI;
        float ri    = 1.8 + h3.z*4.5;
        vec3  ppos0 = vec3(
            (h3.x*2.0-1.0)*3.5,
            (h3.y*2.0-1.0)*2.2,
            (h3.z*2.0-1.0)*2.0
        );

        // Drift outward slowly
        float speed  = 0.18 + h3.z*0.35;
        vec3  vel    = normalize(ppos0)*speed;
        float t0     = hash(fi*13.7)*20.0;   // time offset (stagger)
        float tmod   = mod(iTime+t0, 18.0);
        vec3  ppos   = ppos0 + vel*tmod;
        // wrap: reset when too far
        ppos = mix(ppos0*0.5, ppos, smoothstep(0.0,1.2,tmod));

        // Particle color: yellow / magenta / cyan / blue
        float cIdx = floor(h3.z*4.0);
        vec3 pCol;
        if(cIdx < 1.0)      pCol = vec3(1.0, 0.90, 0.05);  // yellow
        else if(cIdx < 2.0) pCol = vec3(0.82, 0.18, 0.90); // magenta
        else if(cIdx < 3.0) pCol = vec3(0.08, 0.72, 0.92); // cyan
        else                pCol = vec3(0.20, 0.32, 0.95); // blue

        // Particle size proportional to audio
        float fBand = hash(fi*7.3)*0.9+0.05;
        float amp   = spec(fBand);
        float pr    = (0.004 + h3.x*0.008 + amp*0.005);

        // Project to screen
        vec2 sc = toScreen(ppos, ro, uu, vv, ww, fov);
        float dist2 = length(uv - sc);
        if(dist2 < pr*2.5){
            float mask = smoothstep(pr*2.5, pr*0.4, dist2);
            // Depth cue: behind blob = dim
            bool  inFront = !hit || length(ppos-ro) < t;
            float dim = inFront ? 1.0 : 0.28;
            col = mix(col, pCol, mask*dim*0.92);
        }
    }

    col = aces(col*1.05);
    gl_FragColor = vec4(col,1.0);
}
