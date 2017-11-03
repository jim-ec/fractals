#version 450
#extension GL_ARB_separate_shader_objects : enable

out gl_PerVertex {
    vec4 gl_Position;
};

layout(location = 0) out vec2 vPosition;

vec2 positions[6] = vec2[](
    vec2(-1, 1),
    vec2(-1, -1),
    vec2(1, -1),
    vec2(-1, 1),
    vec2(1, -1),
    vec2(1, 1)
);

void main() {
    vPosition = positions[gl_VertexIndex] * 2;
    vPosition.x *= 800.0 / 600.0;
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
}
