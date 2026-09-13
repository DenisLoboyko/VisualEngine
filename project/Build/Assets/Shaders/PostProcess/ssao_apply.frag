#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uScene;
uniform sampler2D uAO;
void main(){
    float t = 1.0/1024.0;
    float ao = (texture(uAO,vUV).r + texture(uAO,vUV+vec2(t,0)).r + texture(uAO,vUV-vec2(t,0)).r + texture(uAO,vUV+vec2(0,t)).r + texture(uAO,vUV-vec2(0,t)).r) / 5.0;
    FragColor = vec4(texture(uScene,vUV).rgb * mix(1.0, ao, 0.85), 1.0);
}
