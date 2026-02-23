// === VERTEX ===
void main() {
  gl_Position = vec4(position.xy, 0.0, 1.0);
}

// === FRAGMENT ===
// Neon Cylinder City  symmetric tunnel (floor + ceiling mirror)

float audioAt(float x) {
  int i = int(clamp(x*128.0,0.0,127.0));
  return clamp(uAudioData[i],0.0,1.0);
}
float bass(){ float s=0.0; for(int i=0;i<8; i++) s+=audioAt(float(i)/128.0); return s/8.0; }
float mid() { float s=0.0; for(int i=8;i<32;i++) s+=audioAt(float(i)/128.0); return s/24.0; }

vec2 hash22(vec2 p){
  vec2 q=vec2(dot(p,vec2(127.1,311.7)),dot(p,vec2(269.5,183.3)));
  return fract(sin(q)*43758.5453);
}
vec3 hsv(float h,float s,float v){
  vec3 c=abs(mod(h*6.0+vec3(0,4,2),6.0)-3.0)-1.0;
  return v*mix(vec3(1.0),clamp(c,0.0,1.0),s);
}
vec3 aces(vec3 x){ return clamp((x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14),0.0,1.0); }

float sdCylinder(vec3 p,float r,float h){
  vec2 d=abs(vec2(length(p.xz),p.y))-vec2(r,h);
  return min(max(d.x,d.y),0.0)+length(max(d,0.0));
}
float sdRoundBox(vec3 p,vec3 b,float r){
  vec3 q=abs(p)-b+r;
  return length(max(q,0.0))+min(max(q.x,max(q.y,q.z)),0.0)-r;
}

const float GRID    =  5.5;
const float FLOOR_Y = -2.5;
const float CEIL_Y  =  2.5;

// matID: 1=floor  2=ceiling  4=floor-cyl  5=box  7=ceil-cyl
vec4 map(vec3 p){
  vec4 res=vec4(1e5,0,0,0);

  // floor & ceiling planes
  float fd=p.y-FLOOR_Y;
  float cd=CEIL_Y-p.y;
  if(fd<res.x) res=vec4(fd,1,0,0);
  if(cd<res.x) res=vec4(cd,2,0,0);

  vec2 gid=floor(p.xz/GRID);
  for(int dz=-1;dz<=1;dz++) for(int dx=-1;dx<=1;dx++){
    vec2 cell=gid+vec2(float(dx),float(dz));

    // floor cylinders grow UP
    vec2 rnd=hash22(cell);
    vec2 cxz=(cell+0.5+(rnd-0.5)*0.7)*GRID;
    if(abs(cxz.x)<9.5){
      float r=0.22+rnd.y*0.18;
      float av=audioAt(fract(rnd.x*13.7));
      float h=0.5+av*5.0;
      float d=sdCylinder(p-vec3(cxz.x,FLOOR_Y+h,cxz.y),r,h);
      if(d<res.x) res=vec4(d,4,rnd.x,r);
    }

    // ceiling cylinders hang DOWN (different hash offset so they don't align)
    vec2 rnd2=hash22(cell+vec2(7.3,2.1));
    vec2 cxz2=(cell+0.5+(rnd2-0.5)*0.7)*GRID;
    if(abs(cxz2.x)<9.5){
      float r2=0.22+rnd2.y*0.18;
      float av2=audioAt(fract(rnd2.x*13.7));
      float h2=0.5+av2*5.0;
      // pivot at ceiling, extend downward
      float d2=sdCylinder(p-vec3(cxz2.x,CEIL_Y-h2,cxz2.y),r2,h2);
      if(d2<res.x) res=vec4(d2,7,rnd2.x,r2);
    }
  }

  // weaving box  raised to mid-tunnel / camera level
  float spd=9.0;
  float bz=-iTime*spd-4.0;
  float bx=sin(iTime*0.52)*2.6+sin(iTime*0.31)*0.9;
  float by=0.0;            // centre of tunnel
  float bv=bass();
  vec3 bp=p-vec3(bx,by+bv*0.18,bz);
  float bd=sdRoundBox(bp,vec3(0.28+bv*0.04,0.18+bv*0.02,0.42),0.04);
  if(bd<res.x) res=vec4(bd,5,0,0);

  return res;
}

vec3 calcNormal(vec3 p){
  const float e=0.001;
  return normalize(vec3(
    map(p+vec3(e,0,0)).x-map(p-vec3(e,0,0)).x,
    map(p+vec3(0,e,0)).x-map(p-vec3(0,e,0)).x,
    map(p+vec3(0,0,e)).x-map(p-vec3(0,0,e)).x));
}
float calcAO(vec3 p,vec3 n){
  float o=0.0,s=1.0;
  for(int i=0;i<6;i++){float h=0.01+0.15*float(i)/5.0;o+=(h-map(p+h*n).x)*s;s*=0.75;}
  return clamp(1.0-2.5*o,0.0,1.0);
}
float softShadow(vec3 ro,vec3 rd,float mint,float maxt){
  float r=1.0,t=mint;
  for(int i=0;i<20;i++){float h=map(ro+rd*t).x;if(h<0.001)return 0.0;r=min(r,8.0*h/t);t+=clamp(h,0.04,0.4);if(t>maxt)break;}
  return clamp(r,0.0,1.0);
}

vec3 cylColor(float ch){float av=audioAt(fract(ch*13.7));return hsv(ch,0.9,0.5+av*0.55);}
vec3 cylGlow (float ch){float av=audioAt(fract(ch*13.7));return hsv(ch,1.0,1.0)*(2.0+av*7.0);}

// colour bleed from both floor and ceiling cylinders onto a surface point
vec3 lightBleed(vec3 p,float occ){
  vec3 bl=vec3(0); vec2 pg=floor(p.xz/GRID);
  for(int dz=-1;dz<=1;dz++) for(int dx=-1;dx<=1;dx++){
    vec2 cell=pg+vec2(float(dx),float(dz));
    // floor cyls
    vec2 rnd=hash22(cell);
    vec2 cxz=(cell+0.5+(rnd-0.5)*0.7)*GRID;
    if(abs(cxz.x)<9.5){float av=audioAt(fract(rnd.x*13.7));bl+=hsv(rnd.x,1.0,1.0)*(av+0.1)*exp(-length(p.xz-cxz)*0.16)*0.6;}
    // ceil cyls
    vec2 rnd2=hash22(cell+vec2(7.3,2.1));
    vec2 cxz2=(cell+0.5+(rnd2-0.5)*0.7)*GRID;
    if(abs(cxz2.x)<9.5){float av2=audioAt(fract(rnd2.x*13.7));bl+=hsv(rnd2.x,1.0,1.0)*(av2+0.1)*exp(-length(p.xz-cxz2)*0.16)*0.6;}
  }
  return bl*occ;
}

vec3 volumetricGlow(vec3 ro,vec3 rd,float tmax){
  vec3 glow=vec3(0); float t=0.5;
  for(int i=0;i<56;i++){
    vec3 p=ro+rd*t;
    vec2 gid=floor(p.xz/GRID);
    for(int dz=-1;dz<=1;dz++) for(int dx=-1;dx<=1;dx++){
      vec2 cell=gid+vec2(float(dx),float(dz));
      // floor cyls
      {vec2 rnd=hash22(cell);vec2 cxz=(cell+0.5+(rnd-0.5)*0.7)*GRID;
       if(abs(cxz.x)<9.5){float av=audioAt(fract(rnd.x*13.7));float h=0.5+av*5.0;float r=0.22+rnd.y*0.18;
        float ax=length(p.xz-cxz)-r;float ay=abs(p.y-(FLOOR_Y+h))-h;float d=max(ax,ay*0.35);
        if(d<4.0) glow+=hsv(rnd.x,1.0,1.0)*(1.0+av*3.5)*exp(-d*0.85)*0.016;}}
      // ceil cyls
      {vec2 rnd2=hash22(cell+vec2(7.3,2.1));vec2 cxz2=(cell+0.5+(rnd2-0.5)*0.7)*GRID;
       if(abs(cxz2.x)<9.5){float av2=audioAt(fract(rnd2.x*13.7));float h2=0.5+av2*5.0;float r2=0.22+rnd2.y*0.18;
        float ax2=length(p.xz-cxz2)-r2;float ay2=abs(p.y-(CEIL_Y-h2))-h2;float d2=max(ax2,ay2*0.35);
        if(d2<4.0) glow+=hsv(rnd2.x,1.0,1.0)*(1.0+av2*3.5)*exp(-d2*0.85)*0.016;}}
    }
    t+=0.85+t*0.045; if(t>tmax) break;
  }
  return glow;
}

// shared grid shading for floor / ceiling
vec3 shadeGrid(vec3 p,vec3 n,vec3 rd,vec3 sun,float dif,float sh,float occ,float mv,bool isCeil){
  vec2 gp=fract(p.xz/GRID);
  float lx=smoothstep(0.93,1.0,gp.x)+smoothstep(0.07,0.0,gp.x);
  float lz=smoothstep(0.93,1.0,gp.y)+smoothstep(0.07,0.0,gp.y);
  float grid=clamp(lx+lz,0.0,1.0);
  vec3 lineCol= isCeil
    ? vec3(0.62,0.22,0.45)*(1.0+mv*1.8)   // ceiling: warm magenta grid
    : vec3(0.22,0.32,0.62)*(1.0+mv*1.8);  // floor:   cool blue grid
  vec3 col=mix(vec3(0.025,0.025,0.05),lineCol,grid*0.9);
  col+=col*dif*sh*0.35;
  col*=occ;
  col+=lightBleed(p,occ);

  // mirror reflection
  vec3 rr=reflect(rd,n);
  float rt=0.02; float rm=0.0,rc=0.0;
  for(int j=0;j<56;j++){
    vec3 rp2=p+rr*rt; vec4 rh2=map(rp2);
    if(rh2.x<0.001*rt){rm=rh2.y;rc=rh2.z;break;}
    if(rt>55.0) break;
    rt+=rh2.x*0.88;
  }
  vec3 rcol=vec3(0);
  if(rm>0.5){
    vec3 rp2=p+rr*rt; vec3 rn2=calcNormal(rp2);
    float rdif=max(dot(rn2,sun),0.0);
    if((rm>1.5&&rm<4.5)||(rm>6.5)){
      rcol=cylColor(rc)*(0.1+rdif*0.4)+cylGlow(rc)*pow(1.0-abs(dot(rn2,-rr)),2.5);
    } else if(rm>4.5&&rm<5.5){
      vec3 bc=vec3(0.35,0.65,1.0);float bf=pow(1.0-abs(dot(rn2,-rr)),2.5);
      rcol=(bc*(0.2+rdif*0.4)+bc*4.5*bf);
    } else if(rm>5.5&&rm<6.5){
      rcol=vec3(0.2,0.5,1.0)*rc*pow(1.0-abs(dot(rn2,-rr)),2.5)*3.0;
    }
    rcol*=exp(-rt*0.055);
  }
  float fres=pow(1.0-max(dot(n,-rd),0.0),5.0);
  col=mix(col,col*0.4+rcol*0.8,fres*0.7);
  return col;
}

void main(){
  vec2 uv=(gl_FragCoord.xy-iResolution.xy*0.5)/iResolution.y;

  float spd=9.0;
  float bv=bass(); float mv=mid();

  // camera centred in tunnel
  float camZ=-iTime*spd;
  float camX=sin(iTime*0.52)*2.6+sin(iTime*0.31)*0.9;
  float camY=sin(iTime*0.21)*0.25+bv*0.1;  // hover around y=0
  vec3 ro=vec3(camX,camY,camZ);

  float la=3.5;
  float ftz=-(iTime+la/spd)*spd;
  float ftx=sin((iTime+la/spd)*0.52)*2.6+sin((iTime+la/spd)*0.31)*0.9;
  vec3 ta=vec3(ftx,0.0,ftz);

  vec3 fwd=normalize(ta-ro);
  vec3 rgt=normalize(cross(vec3(0,1,0),fwd));
  vec3 up=cross(fwd,rgt);
  float roll=bv*0.045;
  rgt=rgt*cos(roll)+up*sin(roll);
  up=cross(fwd,rgt);
  vec3 rd=normalize(uv.x*rgt+uv.y*up+1.65*fwd);

  const int STEPS=120;
  const float TMAX=130.0;
  float t=0.02,mat=0.0,ch=0.0,cr=0.0;
  vec3 hit=ro;
  for(int i=0;i<STEPS;i++){
    hit=ro+rd*t;
    vec4 h=map(hit);
    if(h.x<0.0006*t){mat=h.y;ch=h.z;cr=h.w;break;}
    if(t>TMAX){mat=0.0;break;}
    t+=h.x*0.92;
  }

  vec3 col=vec3(0);
  vec3 sun=normalize(vec3(0.2,1.0,-0.6));

  if(mat>0.5){
    vec3 p=hit;
    vec3 n=calcNormal(p);
    float occ=calcAO(p,n);
    float dif=max(dot(n,sun),0.0);
    float sh=softShadow(p+n*0.004,sun,0.04,25.0);
    float spe=pow(max(dot(reflect(-sun,n),-rd),0.0),64.0);

    if(mat<1.5){
      col=shadeGrid(p,n,rd,sun,dif,sh,occ,mv,false);

    } else if(mat<2.5){
      col=shadeGrid(p,n,rd,sun,dif,sh,occ,mv,true);

    } else if(mat<4.5){
      // floor cylinder
      float fres=pow(1.0-abs(dot(n,-rd)),2.2);
      col=cylColor(ch)*(0.06+dif*sh*0.8)+vec3(1)*spe*sh*0.25;
      col*=occ;
      col+=cylGlow(ch)*fres;
      col+=lightBleed(p,occ)*0.35;

    } else if(mat<5.5){
      // box
      float fres=pow(1.0-abs(dot(n,-rd)),1.8);
      vec3 bc=vec3(0.3,0.65,1.0);
      col=bc*(0.1+dif*sh*0.65)+bc*spe*sh;
      col*=occ;
      col+=bc*(3.5+bv*6.0)*fres;
      col+=vec3(0.5,0.8,1.0)*bv*0.7;

    } else {
      // ceiling cylinder
      float fres=pow(1.0-abs(dot(n,-rd)),2.2);
      col=cylColor(ch)*(0.06+dif*sh*0.8)+vec3(1)*spe*sh*0.25;
      col*=occ;
      col+=cylGlow(ch)*fres;
      col+=lightBleed(p,occ)*0.35;
    }
  } else {
    // sky between cylinders
    float fy=rd.y*0.5+0.5;
    col=mix(vec3(0,0,0.02),vec3(0,0.01,0.06),fy);
  }

  float hitT=(mat>0.5)?t:TMAX;
  col+=volumetricGlow(ro,rd,min(hitT,65.0));
  col=mix(vec3(0,0,0.012),col,exp(-t*0.016));
  col+=vec3(0.25,0.45,1.0)*bv*0.07*exp(-length(uv)*4.0);
  col*=smoothstep(1.4,0.25,length(uv));
  col=aces(col*1.5);
  col=pow(max(col,0.0),vec3(0.4545));
  gl_FragColor=vec4(col,1.0);
}


