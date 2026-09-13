#version 330 core
in vec2 vUV;
out float outAO;
uniform sampler2D uDepth;
uniform mat4 uInvProj;
uniform vec2 uScreen;
uniform float uRadius;
vec3 viewPos(vec2 uv){
    float d = texture(uDepth, uv).r;
    vec4 v = uInvProj * vec4(uv*2.0-1.0, d*2.0-1.0, 1.0);
    return v.xyz / v.w;
}
void main(){
    float d0 = texture(uDepth, vUV).r;
    if (d0 > 0.9999) { outAO = 1.0; return; }
    vec3 P = viewPos(vUV);
    vec3 P1 = viewPos(vUV + vec2(1.0/uScreen.x, 0.0));
    vec3 P2 = viewPos(vUV + vec2(0.0, 1.0/uScreen.y));
    vec3 N = normalize(cross(P1 - P, P2 - P));
    vec3 K[8];
    K[0]=vec3(0.53,0.18,-0.71); K[1]=vec3(-0.22,0.89,-0.41);
    K[2]=vec3(0.81,-0.33,-0.48); K[3]=vec3(-0.67,0.12,-0.73);
    K[4]=vec3(0.19,0.95,-0.24); K[5]=vec3(-0.41,-0.61,-0.67);
    K[6]=vec3(0.62,-0.77,-0.16); K[7]=vec3(-0.09,0.41,-0.91);
    float ao = 0.0;
    for (int i = 0; i < 8; i++) {
        vec3 dir = normalize(K[i] + N * 0.6);
        float len = 0.3 + 0.7 * float(i % 4) / 3.0;
        vec3 S = P + dir * (uRadius * len);
        vec4 off = (uInvProj * vec4(0.0)).w > 0.0 ? vec4(S * 0.5, 1.0) : vec4(0.0);
        vec2 suv = vec2(0.5) + S.xy / (-S.z * 2.0);
        if (suv.x<0.0||suv.x>1.0||suv.y<0.0||suv.y>1.0) continue;
        vec3 SP = viewPos(suv);
        float diff = SP.z - S.z;
        float occ = (diff > 0.02) ? 1.0 : 0.0;
        float rc = 1.0 - smoothstep(uRadius*0.5, uRadius, abs(diff));
        ao += occ * rc;
    }
    outAO = clamp(1.0 - ao/8.0, 0.0, 1.0);
}
