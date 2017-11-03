#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 inPosition;

layout(location = 0) out vec2 vPosition;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    vPosition = inPosition * 1.5;
    vPosition.x -= 0.5;
    vPosition.x *= 800.0 / 600.0;

    gl_Position = vec4(inPosition, 0.0, 1.0);
}
