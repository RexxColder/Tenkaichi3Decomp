#version 450
// Vertex program 0 of the game (128 instructions; the fighters' models), as a shader.
// What the original does per vertex (read from its disassembly, docs/port/README.md):
//   position  two-bone skinning: p = mix(B * (pos - pivotB), A * (pos - pivotA), weight), weight = pos.w
//   screen    s = C * p; the GS gets s.xyz / s.w as 12.4 fixed point (x, y) and an integer depth
//   layer 0   texture coordinates from the vertex, the material colour
//   layer 1   the "toon" layer: u = 0.5 + 0.5 * (light . normal'), v = 0, with the second colour
//   reject    a triangle with a vertex outside the guard volume is dropped (the GPU clips instead)
// Here the position leaves in clip space, so the GPU interpolates with perspective and clips; x and y are mapped
// like gs.vert maps GS pixels, depth is the GS depth over its maximum.
layout(location = 0) in vec4 inPos;    // xyz, weight
layout(location = 1) in vec4 inNormal;
layout(location = 2) in vec4 inSt;     // s, t, 1
layout(location = 0) out vec4 vColor;
layout(location = 1) out vec3 vStq;
layout(set = 1, binding = 0) uniform U {
    mat4 boneA, boneB;   // VU memory 0..3, 4..7
    vec4 pivotA, pivotB; // 8, 9
    mat4 screen;         // 14..17
    vec4 light;          // xyz: the x components of 10..12, w: of 13
    vec4 color0, color1; // 22, 23 (0..255)
    vec4 misc;           // x, y: XYOFFSET in pixels, z: depth maximum, w: layer (0 or 1; 2 = programs 2a / 2b)
} u;
void main() {
    float w = inPos.w;
    vec3 pa = (u.boneA * vec4(inPos.xyz - u.pivotA.xyz, 1.0)).xyz;
    vec3 pb = (u.boneB * vec4(inPos.xyz - u.pivotB.xyz, 1.0)).xyz;
    vec4 s = u.screen * vec4(mix(pb, pa, w), 1.0);
    // Programs 2a / 2b: the shadow camera's matrix is orthographic with a NEGATIVE constant w (measured: -862).
    // The PS2 only divides by it; a GPU would clip everything as "behind the eye". The same point with w > 0:
    if (u.misc.w == 2.0 && s.w < 0.0) s = -s;
    gl_Position = vec4((s.x - u.misc.x * s.w) / 512.0 - s.w, s.w - (s.y - u.misc.y * s.w) / 512.0, s.z / u.misc.z, s.w);
    if (u.misc.w == 2.0) {
        // programs 2a / 2b (flat-colour model for the shadow): their "screen" matrix is the shadow camera's, whose
        // depth is not kept inside the depth range; the PS2 just stores whatever integer comes out. Keep every
        // triangle: depth clamped into range instead of clipped.
        gl_Position.z = clamp(s.z / u.misc.z, 0.0, s.w);
    }
    if (u.misc.w != 1.0) {
        vColor = u.color0 / 255.0;
        vStq = vec3(inSt.xy, 1.0);
    } else {
        float na = dot(u.light.xyz, mat3(u.boneA) * inNormal.xyz), nb = dot(u.light.xyz, mat3(u.boneB) * inNormal.xyz);
        vColor = u.color1 / 255.0;
        vStq = vec3((mix(nb, na, w) + u.light.w) * 0.5 + 0.5, 0.0, 1.0);
    }
}
