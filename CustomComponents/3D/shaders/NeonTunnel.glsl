// === VERTEX ===
void main() {
  gl_Position = vec4(position.xy, 0.0, 1.0);
}

// === FRAGMENT ===
// Neon Wireframe Tunnel  audio-reactive polygon tunnel visualizer

const float PI  = 3.14159265;
const float TAU = 6.28318530;
const int   N_SIDES = 7;   // heptagon (change to 8 for octagon look)
const int   N_RINGS = 28;

float audioAt(float x){
  int i=int(clamp(x*128.0,0.0,127.0));
  return clamp(uAudioData[i],0.0,1.0);
}
float bass()  { float s=0.0; for(int i=0;i<8;  i++) s+=audioAt(float(i)/128.0); return s/8.0; }
float treble(){ float s=0.0; for(int i=48;i<80;i++) s+=audioAt(float(i)/128.0); return s/32.0; }

vec3 hsv(float h,float s,float v){
  vec3 c=abs(mod(h*6.0+vec3(0.0,4.0,2.0),6.0)-3.0)-1.0;
  return v*mix(vec3(1.0),clamp(c,0.0,1.0),s);
}
vec3 aces(vec3 x){
  return clamp((x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14),0.0,1.0);
}

// Distance from p (in unit-circumradius ngon space) to nearest edge
float ngonEdgeDist(vec2 p){
  float r=length(p);
  float a=atan(p.y,p.x);
  float sector=TAU/float(N_SIDES);
  a=mod(a+sector*0.5,sector)-sector*0.5;
  float ir=cos(PI/float(N_SIDES)); // inradius of unit circumradius ngon
  return abs(r*cos(a)-ir);
}

// Distance from p to nearest vertex of unit-circumradius ngon
float ngonVertexDist(vec2 p){
  float a=atan(p.y,p.x);
  float sector=TAU/float(N_SIDES);
  float va=round(a/sector)*sector;
  return length(p-vec2(cos(va),sin(va)));
}

void main(){
  vec2 uv=(gl_FragCoord.xy-iResolution.xy*0.5)/iResolution.y;

  float bv=bass();
  float tv=treble();

  // tunnel scroll speed (bass driven)
  float spd   = 0.9 + bv*0.7;
  float tofs  = fract(iTime*spd);     // smooth ring scroll 0..1
  float twist = iTime*0.07;           // slow whole-tunnel rotation

  vec3 col=vec3(0.0);

  //  rings 
  for(int k=1;k<=N_RINGS;k++){
    float depth = float(k)-tofs;      // effective depth of this ring
    if(depth<0.05) continue;

    float screenR = 0.52/depth;       // perspective screen radius
    float fade    = exp(-depth*0.16);
    if(fade<0.001) continue;

    // per-ring audio frequency bin (stagger so each ring uses different band)
    float af = float((k*5)%64)/64.0;
    float av = audioAt(af);
    screenR *= (1.0+av*0.18);         // audio bulge

    // hue: cyan (0.56) at far depth  magenta (0.83) near camera
    float depthT = 1.0-float(k)/float(N_RINGS);
    float hue = mix(0.56, 0.83, depthT);

    // per-ring rotation accumulates with depth for torsion effect
    float ringTwist = twist + depth*0.12;
    float ca=cos(ringTwist), sa=sin(ringTwist);
    vec2 ruv = vec2(uv.x*ca-uv.y*sa, uv.x*sa+uv.y*ca);
    vec2 puv = ruv/screenR;           // normalised to unit circumradius

    // edge glow (thin wire)
    float ed  = ngonEdgeDist(puv)*screenR;
    float ew  = 0.0016+av*0.0012;
    col += hsv(hue,0.88,1.0)*exp(-ed/ew)*fade*(1.0+av*1.8);

    // vertex node glow (bright dots at corners)
    float vd  = ngonVertexDist(puv)*screenR;
    float vw  = 0.0018+av*0.0012;
    col += hsv(hue,0.35,1.6)*exp(-vd/vw)*fade*(1.0+av*2.5);
  }

  //  longitudinal spoke lines 
  // One spoke per vertex, all radiating from screen centre
  {
    float a      = atan(uv.y,uv.x)-twist;
    float sector = TAU/float(N_SIDES);
    float modA   = mod(a+sector*0.5,sector)-sector*0.5;
    float r      = length(uv);
    // perpendicular distance to nearest spoke direction
    float spokeDist = r*abs(sin(modA));
    float sw        = 0.0011+tv*0.0016;
    // brightness fades away from centre (converging in perspective)
    float spokeFade = exp(-r*2.2)*(0.55+tv*1.8);
    col += vec3(0.05,0.82,1.0)*exp(-spokeDist/sw)*spokeFade;
  }

  //  centre glow (magenta hotspot) 
  float r=length(uv);
  col += hsv(0.82,0.8,1.0)*exp(-r*14.0)*(0.7+bv*3.5);
  col += hsv(0.78,0.6,1.0)*exp(-r* 5.5)*0.35;

  //  bass pulse ring flash 
  // Every strong beat: brief bright ring near camera
  float flashR  = 0.05+bv*0.12;
  float flashD  = abs(r-flashR);
  col += vec3(0.4,0.6,1.0)*exp(-flashD/0.008)*bv*3.5;

  //  vignette 
  col *= smoothstep(1.25,0.15,r);

  //  tone map + gamma 
  col = aces(col*1.3);
  col = pow(max(col,0.0),vec3(0.4545));

  gl_FragColor=vec4(col,1.0);
}

