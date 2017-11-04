#version 450
#extension GL_ARB_separate_shader_objects : enable

const int N = 500;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 fractalTransform;
    vec4 colors[40];
} ubo;

layout(location = 0) in vec2 vPosition;

layout(location = 0) out vec4 outColor;

vec2 multiplyComplex(vec2 a, vec2 b) {
    float real = (a.x * b.x) - (a.y * b.y);
    float imag = (a.x * b.y) + (a.y * b.x);
    return vec2(real, imag);
}

void main() {
    vec2 z = vPosition;
    int n = 0;

    bool withinMandelbrot = true;
    for(; n < N; n++) {
        z = multiplyComplex(z, z) + vPosition;
        if(length(z) > 2) {
            withinMandelbrot = false;
            break;
        }
    }

    if(withinMandelbrot) {
        outColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
    else {
        outColor = ubo.colors[n % 40];
    }
}
