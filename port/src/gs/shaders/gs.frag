#version 450
// GS primitive, fragment stage: texture function (TEX0.TFX / TCC) and alpha test (TEST), as the GS does them.
// Colours are 0..1 for 0..255. Alpha is carried with 1.0 = 0x80 (the GS's "opaque").
layout(location = 0) in vec4 vColor;
layout(location = 1) in vec3 vStq;
layout(location = 0) out vec4 outColor;
layout(set = 2, binding = 0) uniform sampler2D tex;
layout(set = 3, binding = 0) uniform Params {
    ivec4 mode;  // x: textured, y: TFX, z: TCC, w: alpha test (0 off, else ATST + 1)
    vec4 misc;   // x: AREF in GS units (0..255)
} p;
void main() {
    float k = 255.0 / 128.0;
    vec3 rgb = vColor.rgb;
    float a = vColor.a * k;
    if (p.mode.x != 0) {
        vec4 t = texture(tex, vStq.xy / vStq.z);
        if (p.mode.y == 0) {
            rgb = t.rgb * vColor.rgb * k;
            if (p.mode.z != 0) a = t.a * a;
        } else if (p.mode.y == 1) {
            rgb = t.rgb;
            if (p.mode.z != 0) a = t.a;
        } else {
            rgb = t.rgb * vColor.rgb * k + vec3(vColor.a);
            if (p.mode.z != 0) a = (p.mode.y == 2) ? t.a + a : t.a;
        }
    }
    if (p.mode.w != 0) {
        float ag = a * 128.0;
        int f = p.mode.w - 1;
        bool pass = f == 0 ? false : f == 1 ? true : f == 2 ? ag < p.misc.x : f == 3 ? ag <= p.misc.x :
                    f == 4 ? abs(ag - p.misc.x) < 0.5 : f == 5 ? ag >= p.misc.x : f == 6 ? ag > p.misc.x : abs(ag - p.misc.x) >= 0.5;
        if (!pass) discard;
    }
    outColor = vec4(clamp(rgb, 0.0, 1.0), clamp(a, 0.0, 1.0));
}
