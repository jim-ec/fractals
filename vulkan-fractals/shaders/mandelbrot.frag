#version 450
#extension GL_ARB_separate_shader_objects : enable

const int N = 50;
const vec4 EDGE_COLOR = vec4(1.0, 0.0, 0.0, 1.0);

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

    float nf = float(n) / float(N);

    if(withinMandelbrot) {
        outColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
    else {
        outColor = mix(vec4(1.0, 0.0, 1.0, 1.0), EDGE_COLOR, nf);
    }
}
