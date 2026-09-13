#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uHdr;
uniform float uThreshold;
void main(){
    vec3 c = texture(uHdr, vUV).rgb;
    float br = dot(c, vec3(0.2126, 0.7152, 0.0722));
    FragColor = br > uThreshold ? vec4(c, 1.0) : vec4(0.0);
}
