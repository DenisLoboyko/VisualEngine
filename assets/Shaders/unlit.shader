// Unlit: flat color, no lighting
[vertex]
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
uniform mat4 uProj; uniform mat4 uView; uniform mat4 uModel;
void main(){ gl_Position=uProj*uView*uModel*vec4(aPos,1.0); }
[fragment]
#version 330 core
uniform vec3 uBaseColor; uniform float uTime;
out vec4 frag;
void main(){ frag=vec4(uBaseColor,1.0); }
