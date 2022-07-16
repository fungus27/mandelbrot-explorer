#version 460 core
in vec2 position;
out vec4 color;

float max_iters = 100.0f;

unsigned int escape_iters() {
    float c_real = position.x * 2.0f;
    float c_complex = position.y * 2.0f;
    float z_real = 0.0f;
    float z_complex = 0.0f;
    unsigned int i;
    for (i = 0; i < max_iters; i++) {
        float z_real_temp = z_real;
        z_real = z_real*z_real - z_complex*z_complex + c_real;
        z_complex = 2*z_real_temp*z_complex + c_complex;
        if (z_real*z_real + z_complex*z_complex > 4.0f)
            return i;
    }
    return i;
}

void main() {
    unsigned int iters = escape_iters();
    color = vec4(iters/max_iters, 0.0f, 1-iters/max_iters, 1.0f);
}
