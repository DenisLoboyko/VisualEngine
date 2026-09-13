#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uSrc;
uniform vec2 uDir;     // (1.0/W, 0) или (0, 1.0/H)
uniform vec2 uTexel;
// 9-tap Gaussian
void main(){
    vec2 t = uTexel;
    vec2 d = uDir;
    vec3 c = vec3(0.0);
    c += texture(uSrc, vUV + d * -4.0 * t).rgb * 0.0162162162;
    c += texture(uSrc, vUV + d * -3.0 * t).rgb * 0.0540540541;
    c += texture(uSrc, vUV + d * -2.0 * t).rgb * 0.1216216216;
    c += texture(uSrc, vUV + d * -1.0 * t).rgb * 0.1945945946;
    c += texture(uSrc, vUV                 ).rgb * 0.2270270270;
    c += texture(uSrc, vUV + d *  1.0 * t).rgb * 0.1945945946;
    c += texture(uSrc, vUV + d *  2.0 * t).rgb * 0.1216216216;
    c += texture(uSrc, vUV + d *  3.0 * t).rgb * 0.0540540541;
    c += texture(uSrc, vUV + d *  4.0 * t).rgb * 0.0162162162;
    FragColor = vec4(c, 1.0);
}
