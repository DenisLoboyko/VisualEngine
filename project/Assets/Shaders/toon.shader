// Toon: cel-shading, 4 light bands
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
  vec3 n=normalize(N);
  float d=dot(n,normalize(uSunDir));
  float band = d>0.6?1.0 : d>0.2?0.7 : d>-0.1?0.45 : 0.3;
  vec3 col = uBaseColor*(uAmbient + uSunColor*uSunIntensity*band);
  col = pow(col, vec3(0.4545));
  frag=vec4(col,1.0);
}
