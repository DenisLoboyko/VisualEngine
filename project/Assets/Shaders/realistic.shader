// Realistic: GGX specular + fresnel + soft ambient
[vertex]
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
uniform mat4 uProj; uniform mat4 uView; uniform mat4 uModel;
out vec3 N; out vec3 W;
void main(){ vec4 w=uModel*vec4(aPos,1.0); W=w.xyz; N=mat3(uModel)*aNormal; gl_Position=uProj*uView*w; }
[fragment]
#version 330 core
in vec3 N; in vec3 W;
uniform vec3 uCamPos; uniform vec3 uSunDir; uniform vec3 uSunColor;
uniform float uSunIntensity; uniform vec3 uAmbient; uniform vec3 uBaseColor; uniform float uTime;
out vec4 frag;
void main(){
  vec3 n=normalize(N); vec3 v=normalize(uCamPos-W); vec3 l=normalize(uSunDir); vec3 h=normalize(v+l);
  float NdL=max(dot(n,l),0.0); float NdH=max(dot(n,h),0.0); float VdH=max(dot(v,h),0.0);
  float rough=0.35; float a=rough*rough; float a2=a*a;
  float d=NdH*NdH*(a2-1.0)+1.0; float D=a2/(3.14159*d*d);
  float F=0.04+0.96*pow(1.0-VdH,5.0);
  vec3 diff=uBaseColor*NdL;
  vec3 spec=vec3(D*F*0.25)*NdL;
  vec3 col=diff*uSunColor*uSunIntensity + spec*uSunIntensity + uBaseColor*uAmbient;
  col=clamp((col*(2.51*col+0.03))/(col*(2.43*col+0.59)+0.14),0.0,1.0);
  col=pow(col,vec3(0.4545));
  frag=vec4(col,1.0);
}
