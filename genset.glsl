#version 460 core
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout (r8ui, binding = 0) uniform uimage2D img;

float max_iters = 100.f;

unsigned int escape_iters() {
    float c_real = (gl_GlobalInvocationID.x/640.0f * 2.0f - 1.0f) - 1.0f;
    float c_complex = (gl_GlobalInvocationID.y/480.0f * 2.0f - 1.0f);
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
    imageStore(img, ivec2(gl_GlobalInvocationID.xy), uvec4(int(iters/max_iters * 255), 0, 0, 1));
}
