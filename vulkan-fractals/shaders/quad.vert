#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 fractalTransform;
} ubo;

layout(location = 0) in vec2 inPosition;

layout(location = 0) out vec2 vPosition;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 0.0, 1.0);
    vPosition = gl_Position.xy;
    vPosition.x *= ubo.fractalTransform.x;
    vPosition.y *= ubo.fractalTransform.y;
    vPosition.x += ubo.fractalTransform.z;
    vPosition.y += ubo.fractalTransform.w;
}
