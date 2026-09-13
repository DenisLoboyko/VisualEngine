#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform float uIntensity;
// Простой Reinhard tonemap
vec3 tonemap(vec3 x){ return x / (x + vec3(1.0)); }
void main(){
    vec3 scene = texture(uScene, vUV).rgb;
    vec3 bloom = texture(uBloom, vUV).rgb;
    vec3 hdr = scene + bloom * uIntensity;
    vec3 mapped = tonemap(hdr);
    // gamma
    mapped = pow(mapped, vec3(1.0/2.2));
    FragColor = vec4(mapped, 1.0);
}
