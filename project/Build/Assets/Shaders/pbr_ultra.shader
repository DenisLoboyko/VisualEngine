// PBR Ultra: physically-based rendering, soft PCF shadows, hemisphere sky
[vertex]
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUv;
uniform mat4 uProj; uniform mat4 uView; uniform mat4 uModel; uniform mat4 lightSpaceMatrix;
out vec3 N; out vec3 W; out vec2 vUv; out vec4 FLS;
void main(){ vec4 w=uModel*vec4(aPos,1.0); W=w.xyz; N=mat3(uModel)*aNormal; vUv=aUv; FLS=lightSpaceMatrix*w; gl_Position=uProj*uView*w; }
[fragment]
#version 330 core
in vec3 N; in vec3 W; in vec2 vUv; in vec4 FLS;
uniform vec3 uCamPos; uniform vec3 uSunDir; uniform vec3 uSunColor; uniform float uSunIntensity;
uniform vec3 uAmbient; uniform vec3 uBaseColor; uniform float uTime;
uniform sampler2D shadowMap; uniform sampler2D uTexture; uniform int useTexture;
out vec4 frag;
float ShadowPCF(vec3 n, vec3 l){
  vec3 p = FLS.xyz/FLS.w; p=p*0.5+0.5;
  if(p.x<0.0||p.x>1.0||p.y<0.0||p.y>1.0||p.z>1.0) return 1.0;
  float bias = max(0.005*(1.0-dot(n,l)),0.002);
  float s=0.0; vec2 tx=1.0/textureSize(shadowMap,0);
  for(int x=-1;x<=1;x++)for(int y=-1;y<=1;y++){
    float d=texture(shadowMap,p.xy+vec2(x,y)*tx).r;
    s += (p.z-bias > d) ? 0.0 : 1.0;
  }
  return s/9.0;
}
void main(){
  vec3 n=normalize(N); vec3 v=normalize(uCamPos-W); vec3 l=normalize(uSunDir); vec3 h=normalize(v+l);
  vec3 base=uBaseColor;
  if(useTexture==1) base *= texture(uTexture, vUv).rgb;
  float rough=0.35; float metal=0.0;
  float NdL=max(dot(n,l),0.0); float NdH=max(dot(n,h),0.0);
  float NdV=max(dot(n,v),0.001); float VdH=max(dot(v,h),0.0);
  float a=rough*rough; float a2=a*a;
  float D=a2/(3.14159*pow(NdH*NdH*(a2-1.0)+1.0,2.0));
  float G=(NdV/(NdV*0.5+0.5))*(NdL/(NdL*0.5+0.5));
  vec3 F=vec3(0.04)+0.96*pow(1.0-VdH,5.0);
  vec3 spec=D*G*F/(4.0*NdV*NdL+0.001);
  float sh=ShadowPCF(n,l);
  vec3 col = base * uAmbient * (0.6+0.4*n.y);
  col += (base*(1.0-metal)*NdL + spec*NdL) * uSunColor * uSunIntensity * sh;
  col += base * mix(vec3(0.10,0.09,0.08), vec3(0.25,0.35,0.50), n.y*0.5+0.5) * 0.35;
  frag=vec4(col,1.0);
}
