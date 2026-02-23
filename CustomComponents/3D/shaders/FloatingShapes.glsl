// === VERTEX ===
void main() { gl_Position = vec4(position.xy, 0.0, 1.0); }

// === FRAGMENT ===
// Floating Shapes  Memphis-style audio-reactive 3D scene

float audioAt(float x){int i=int(clamp(x*128.0,0.0,127.0));return clamp(uAudioData[i],0.0,1.0);}
float bv(){float s=0.0;for(int i=0;i<8;i++) s+=audioAt(float(i)/128.0);return s/8.0;}
float mv(){float s=0.0;for(int i=8;i<32;i++)s+=audioAt(float(i)/128.0);return s/24.0;}

vec3 aces(vec3 x){return clamp((x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14),0.0,1.0);}

float sdSph(vec3 p,float r){return length(p)-r;}
float sdTorus(vec3 p,float R,float r){return length(vec2(length(p.xz)-R,p.y))-r;}
float sdCap(vec3 p,vec3 a,vec3 b,float r){vec3 ab=b-a,ap=p-a;float t=clamp(dot(ap,ab)/dot(ab,ab),0.0,1.0);return length(ap-t*ab)-r;}
float sdPrism(vec3 p,vec2 h){vec3 q=abs(p);return max(q.z-h.y,max(q.x*0.866025+p.y*0.5,-p.y)-h.x*0.5);}

mat2 R2(float a){float c=cos(a),s=sin(a);return mat2(c,-s,s,c);}
vec3 rX(vec3 p,float a){p.yz=R2(a)*p.yz;return p;}
vec3 rY(vec3 p,float a){p.xz=R2(a)*p.xz;return p;}
vec3 rZ(vec3 p,float a){p.xy=R2(a)*p.xy;return p;}

const vec3 BG=vec3(0.58,0.35,0.80);

vec2 map(vec3 p){
  float t=iTime, b=bv();
  vec2 res=vec2(1e5,0.0);

  // large striped sphere (center)
  vec3 c0=vec3(0.3+sin(t*0.37)*0.12, 0.4+sin(t*0.41)*0.14+b*0.20, sin(t*0.29)*0.08);
  float d;
  d=sdSph(p-c0,1.0+b*0.04); if(d<res.x) res=vec2(d,3.0);

  // large dotted sphere
  vec3 c1=vec3(-0.1+sin(t*0.31)*0.10,-0.8+sin(t*0.35)*0.12+b*0.18,sin(t*0.27)*0.07-0.2);
  d=sdSph(p-c1,0.72+b*0.03); if(d<res.x) res=vec2(d,4.0);

  // orange spheres (4)
  vec3 o0=vec3(-1.8+sin(t*0.43)*0.10, 1.0+sin(t*0.39)*0.15+b*0.22, sin(t*0.33)*0.06);
  d=sdSph(p-o0,0.44); if(d<res.x) res=vec2(d,1.0);

  vec3 o1=vec3(-1.5+sin(t*0.38)*0.12,-1.1+sin(t*0.44)*0.10+b*0.18, 0.1+sin(t*0.31)*0.08);
  d=sdSph(p-o1,0.40); if(d<res.x) res=vec2(d,1.0);

  vec3 o2=vec3( 1.9+sin(t*0.35)*0.09,-0.3+sin(t*0.42)*0.13+b*0.16, 0.2+sin(t*0.28)*0.07);
  d=sdSph(p-o2,0.36); if(d<res.x) res=vec2(d,1.0);

  vec3 o3=vec3(-0.2+sin(t*0.40)*0.08,-1.9+sin(t*0.36)*0.10+b*0.14, 0.4+sin(t*0.30)*0.06);
  d=sdSph(p-o3,0.28); if(d<res.x) res=vec2(d,1.0);

  // glass spheres (3)
  vec3 g0=vec3(-0.9+sin(t*0.34)*0.10, 0.2+sin(t*0.38)*0.12+b*0.12, 0.8+sin(t*0.27)*0.08);
  d=sdSph(p-g0,0.52); if(d<res.x) res=vec2(d,2.0);

  vec3 g1=vec3( 2.1+sin(t*0.41)*0.12, 0.9+sin(t*0.35)*0.14+b*0.15, sin(t*0.32)*0.07);
  d=sdSph(p-g1,0.44); if(d<res.x) res=vec2(d,2.0);

  vec3 g2=vec3( 1.2+sin(t*0.36)*0.08, 1.9+sin(t*0.43)*0.10+b*0.10,-0.1+sin(t*0.29)*0.06);
  d=sdSph(p-g2,0.26); if(d<res.x) res=vec2(d,2.0);

  // striped flat prism  left
  vec3 pr0=vec3(-2.3+sin(t*0.32)*0.10,-0.4+sin(t*0.37)*0.12+b*0.16, 0.1);
  {vec3 lp=rZ(rX(p-pr0,-0.15),0.25); d=sdPrism(lp,vec2(0.72,0.16)); if(d<res.x) res=vec2(d,5.0);}

  // striped flat prism  right
  vec3 pr1=vec3( 2.4+sin(t*0.39)*0.11, 0.2+sin(t*0.33)*0.10+b*0.14, sin(t*0.28)*0.06);
  {vec3 lp=rZ(rX(p-pr1, 0.12),-0.38); d=sdPrism(lp,vec2(0.66,0.16)); if(d<res.x) res=vec2(d,5.0);}

  // springs / torus (maroon)
  vec3 sp0=vec3(-0.9+sin(t*0.45)*0.09,-1.4+sin(t*0.38)*0.11+b*0.20, 0.3);
  {vec3 lp=rX(p-sp0,0.5); d=sdTorus(lp,0.34,0.11); if(d<res.x) res=vec2(d,6.0);}

  vec3 sp1=vec3( 0.7+sin(t*0.37)*0.10, 0.5+sin(t*0.41)*0.12+b*0.15,-1.0);
  {vec3 lp=rY(rX(p-sp1,0.4),0.8); d=sdTorus(lp,0.28,0.09); if(d<res.x) res=vec2(d,6.0);}

  // white S-tubes
  vec3 tb0=vec3(-0.4+sin(t*0.33)*0.08, 1.7+sin(t*0.40)*0.10+b*0.12, 0.3);
  {vec3 lp=p-tb0;
   d=min(sdCap(lp,vec3(-0.22, 0.25,0.0),vec3( 0.08, 0.0,0.0),0.065),
         sdCap(lp,vec3( 0.08, 0.0,0.0),vec3(-0.08,-0.15,0.0),0.065));
   d=min(d,sdCap(lp,vec3(-0.08,-0.15,0.0),vec3( 0.22,-0.38,0.0),0.065));
   if(d<res.x) res=vec2(d,7.0);}

  vec3 tb1=vec3( 1.2+sin(t*0.36)*0.09,-1.6+sin(t*0.43)*0.10+b*0.10,-0.1);
  {vec3 lp=rY(p-tb1,0.6);
   d=min(sdCap(lp,vec3(-0.22, 0.25,0.0),vec3( 0.08, 0.0,0.0),0.065),
         sdCap(lp,vec3( 0.08, 0.0,0.0),vec3(-0.08,-0.15,0.0),0.065));
   d=min(d,sdCap(lp,vec3(-0.08,-0.15,0.0),vec3( 0.22,-0.38,0.0),0.065));
   if(d<res.x) res=vec2(d,7.0);}

  return res;
}

vec3 calcN(vec3 p){
  const float e=0.001;
  return normalize(vec3(
    map(p+vec3(e,0,0)).x-map(p-vec3(e,0,0)).x,
    map(p+vec3(0,e,0)).x-map(p-vec3(0,e,0)).x,
    map(p+vec3(0,0,e)).x-map(p-vec3(0,0,e)).x));
}
float calcAO(vec3 p,vec3 n){
  float o=0.0,s=1.0;
  for(int i=0;i<5;i++){float h=0.02+0.1*float(i)/4.0;o+=(h-map(p+h*n).x)*s;s*=0.80;}
  return clamp(1.0-2.0*o,0.0,1.0);
}
float softShadow(vec3 ro,vec3 rd,float mint,float maxt){
  float r=1.0,t=mint;
  for(int i=0;i<18;i++){float h=map(ro+rd*t).x;if(h<0.001)return 0.0;
    r=min(r,6.0*h/t);t+=clamp(h,0.04,0.5);if(t>maxt)break;}
  return clamp(r,0.0,1.0);
}

void main(){
  vec2 uv=(gl_FragCoord.xy-iResolution.xy*0.5)/iResolution.y;

  vec3 ro=vec3(0.0,0.0,5.8);
  vec3 rd=normalize(vec3(uv,-1.85));

  const int STEPS=90;
  float t=0.05; float mat=0.0; vec3 hit=ro;
  for(int i=0;i<STEPS;i++){
    hit=ro+rd*t;
    vec2 h=map(hit);
    if(h.x<0.0005*t){mat=h.y;break;}
    if(t>18.0){mat=0.0;break;}
    t+=h.x*0.90;
  }

  vec3 col=BG*(0.90+uv.y*0.25);

  if(mat>0.5){
    vec3 n=calcN(hit);
    float occ =calcAO(hit,n);
    vec3  sun =normalize(vec3(-0.6,1.0,-0.5));
    vec3  fill=normalize(vec3( 0.5,-0.3,1.0));
    float dif =max(dot(n,sun),0.0);
    float fil =max(dot(n,fill),0.0)*0.28;
    float sh  =softShadow(hit+n*0.006,sun,0.06,9.0);
    float spe =pow(max(dot(reflect(-sun,n),-rd),0.0),52.0);
    float spe2=pow(max(dot(reflect(-fill,n),-rd),0.0),32.0)*0.2;
    float fres=pow(1.0-abs(dot(n,-rd)),3.5);

    if(mat<1.5){
      // orange
      col=vec3(0.95,0.42,0.07)*(0.12+(dif*sh+fil)*0.88)+(vec3(1.0)*spe*sh+vec3(1.0,0.8,0.6)*spe2)*0.7;
      col*=occ;
    }
    else if(mat<2.5){
      // glass: Fresnel rim + semi-transparent interior
      vec3 inner=vec3(0.82,0.84,0.92)*(0.08+(dif*sh+fil)*0.25);
      vec3 rim=vec3(1.0)*spe*1.1;
      col=mix(inner+BG*0.18, vec3(1.0), fres*0.88)+rim;
      col*=occ*0.92+0.08;
    }
    else if(mat<3.5){
      // striped sphere  use normal for spherical UV
      float lat=asin(clamp(n.y,-1.0,1.0));
      float lon=atan(n.z,n.x);
      float stripe=step(0.5,fract((lat+lon)*2.6));
      vec3 sc=mix(vec3(0.07),vec3(0.96),stripe);
      col=sc*(0.12+(dif*sh+fil)*0.88)+vec3(1.0)*spe*sh*0.55;
      col=mix(col,vec3(1.0)*spe,fres*0.25);
      col*=occ;
    }
    else if(mat<4.5){
      // dotted sphere
      float lat=asin(clamp(n.y,-1.0,1.0));
      float lon=atan(n.z,n.x);
      vec2 cell=fract(vec2(lon*8.0/3.14159,lat*5.0/1.5708))-0.5;
      float dot=1.0-smoothstep(0.24,0.30,length(cell));
      vec3 dc=mix(vec3(0.93),vec3(0.14,0.10,0.18),dot);
      col=dc*(0.12+(dif*sh+fil)*0.88)+vec3(1.0)*spe*sh*0.40;
      col*=occ;
    }
    else if(mat<5.5){
      // striped prism  stripes along world Y
      float stripe=step(0.5,fract(hit.y*4.2));
      vec3 sc=mix(vec3(0.06),vec3(0.95),stripe);
      col=sc*(0.14+(dif*sh+fil)*0.86)+vec3(1.0)*spe*sh*0.30;
      col*=occ;
    }
    else if(mat<6.5){
      // spring  dark maroon glossy
      col=vec3(0.40,0.05,0.14)*(0.08+(dif*sh+fil)*0.92)+vec3(0.9,0.5,0.55)*spe*sh*0.9;
      col+=vec3(0.7,0.3,0.4)*fres*0.5;
      col*=occ;
    }
    else{
      // white tube
      col=vec3(0.96)*(0.15+(dif*sh+fil)*0.85)+vec3(1.0)*spe*sh*0.5;
      col+=vec3(0.85,0.87,1.0)*fres*0.4;
      col*=occ;
    }
    // contact shadow / fog
    col=mix(BG*0.7,col,exp(-t*0.04));
  }

  // subtle vignette
  col*=smoothstep(1.4,0.2,length(uv));
  col=aces(col*1.1);
  col=pow(max(col,0.0),vec3(0.4545));
  gl_FragColor=vec4(col,1.0);
}
