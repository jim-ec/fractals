#version 450
#extension GL_ARB_separate_shader_objects : enable

const int N = 500;
const int COLOR_COUNT = 4;
const vec4 COLORS[COLOR_COUNT] = vec4[](
    vec4(1.0, 1.0, 1.0, 1.0),
    vec4(0.070588235, 0.137254902, 0.588235294, 1.0),
    vec4(0.97254902, 0.854901961, 0.321568627, 1.0),
    vec4(0.341176471, 0.054901961, 0.141176471, 1.0)
);

layout(location = 0) in highp vec2 vPosition;

layout(location = 0) out vec4 outColor;

highp vec2 multiplyComplex(highp vec2 a, highp vec2 b) {
    double real = (a.x * b.x) - (a.y * b.y);
    double imag = (a.x * b.y) + (a.y * b.x);
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
        outColor = COLORS[n % COLOR_COUNT];
    }
}
