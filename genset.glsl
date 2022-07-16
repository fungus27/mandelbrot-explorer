#version 460 core
layout(local_size_x = 30, local_size_y = 30, local_size_z = 1) in;

layout (r8ui, binding = 0) uniform uimage2D img;
uniform dvec2 bottom_left;
uniform dvec2 upper_right;

uniform float max_iters;
uniform double mag;
uniform dvec2 offset;

unsigned int escape_iters(dvec2 c) {
    dvec2 z = dvec2(0.0, 0.0);
    dvec2 z_sq = dvec2(0.0, 0.0);

    unsigned int i;
    for (i = 0; i < max_iters; i++) {
        z = dvec2(z_sq.x - z_sq.y + c.x, 2 * z.x * z.y + c.y);

        z_sq = z * z;
        if (z_sq.x + z_sq.y > 4.0f)
            return i;
    }
    return i;
}

void main() {
    dvec2 new_coord = (vec2(gl_GlobalInvocationID.xy)/imageSize(img).xy - dvec2(0.5, 0.5)) / mag + dvec2(0.5f, 0.5f);
    dvec2 c = new_coord * (upper_right - bottom_left) + bottom_left + offset;
    unsigned int iters = escape_iters(c);

    imageStore(img, ivec2(gl_GlobalInvocationID.xy), uvec4(int(iters/float(max_iters) * 255), 0, 0, 1));
}
