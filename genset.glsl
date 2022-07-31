#version 460 core
layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout (r8ui, binding = 0) uniform uimage2D img;

uniform unsigned int max_iters;
uniform dvec2 mag;
uniform dvec2 offsetx;
uniform dvec2 offsety;

uniform unsigned int antialiasing;

const double SPLITTER = (1 << 29) + 1;


precise dvec2 ds_set(double a)
{
    precise dvec2 z;
    z.x = a;
    z.y = 0.0;
    return z;
}

precise dvec2 ds_add (precise dvec2 dsa, precise dvec2 dsb)
{
    precise dvec2 dsc;
    precise double t1, t2, e;

    t1 = dsa.x + dsb.x;
    e = t1 - dsa.x;
    t2 = ((dsb.x - e) + (dsa.x - (t1 - e))) + dsa.y + dsb.y;

    dsc.x = t1 + t2;
    dsc.y = t2 - (dsc.x - t1);
    return dsc;
}

precise dvec2 quick_two_sum(precise double a, precise double b)
{
    precise dvec2 temp;
    temp.x = a + b;
    temp.y = b - (temp.x - a);
    return temp;
}

dvec2 Split64(double d)
{
    double t = d * SPLITTER;
    dvec2 result;
    result.x = t - (t - d);
    result.y = d - result.x;

    return result;
}

precise dvec2 twoProd(precise double a, precise double b) {
    precise dvec2 p;
    p.x = a * b;
    precise dvec2 aS = Split64(a);
    precise dvec2 bS = Split64(b);
    p.y = (aS.x * bS.x - p.x) + aS.x * bS.y + aS.y * bS.x + aS.y * bS.y;
    return p;
}

precise dvec2 ds_mul(precise dvec2 a, precise dvec2 b)
{
    precise dvec2 p;

    p = twoProd(a.x, b.x);
    p.y += a.x * b.y + a.y * b.x;
    p = quick_two_sum(p.x, p.y);
    return p;
}


precise dvec2 ds_div(precise dvec2 b, precise dvec2 a) {
    precise double xn = 1.0 / a.x;
    precise double yn = b.x * xn;
    precise double diff = (ds_add(b, -ds_mul(a, dvec2(yn, 0.0)))).x;
    precise dvec2 prod = twoProd(xn, diff);
    return ds_add(dvec2(yn, 0.0), prod);
}

precise unsigned int escape_iters(dvec2 cx, dvec2 cy, unsigned int m_iters) {
    dvec2 zx = dvec2(0.0, 0.0);
    dvec2 zy = dvec2(0.0, 0.0);
    dvec2 z_sqx = dvec2(0.0, 0.0);
    dvec2 z_sqy = dvec2(0.0, 0.0);

    unsigned int i;
    for (i = 0; i < m_iters && z_sqx.x + z_sqy.x < 4.0; i++) {
        zy = ds_add(ds_mul(ds_add(zx, zx), zy), cy);
        zx = ds_add(ds_add(z_sqx, -z_sqy), cx);

        z_sqx = ds_mul(zx, zx);
        z_sqy = ds_mul(zy, zy);
    }
    return i;
}

precise void main() {
    if (antialiasing < 2) {
        dvec2 new_coordx = ds_add(ds_div(ds_set(double(gl_GlobalInvocationID.x)/imageSize(img).x - 0.5), mag), ds_set(0.5));
        dvec2 new_coordy = ds_add(ds_div(ds_set(double(gl_GlobalInvocationID.y)/imageSize(img).x - 0.5), mag), ds_set(0.5));
        dvec2 cx = ds_add(new_coordx, ds_add(offsetx, ds_set(-0.5)));
        dvec2 cy = ds_add(new_coordy, ds_add(offsety, ds_set(-0.5)));
        unsigned int iters = escape_iters(cx, cy, max_iters);

        imageStore(img, ivec2(gl_GlobalInvocationID.xy), uvec4(int(iters/float(max_iters) * 255), 0, 0, 255));
    }
    else {
        unsigned int lowest_iters = max_iters;
        for (unsigned int x = 0; x < antialiasing; ++x) {
            for (unsigned int y = 0; y < antialiasing; ++y) {
                dvec2 new_coordx = ds_add(ds_div(ds_set(double(gl_GlobalInvocationID.x + x * 1.0/antialiasing)/imageSize(img).x - 0.5), mag), ds_set(0.5));
                dvec2 new_coordy = ds_add(ds_div(ds_set(double(gl_GlobalInvocationID.y + y * 1.0/antialiasing)/imageSize(img).x - 0.5), mag), ds_set(0.5));
                dvec2 cx = ds_add(new_coordx, ds_add(offsetx, ds_set(-0.5)));
                dvec2 cy = ds_add(new_coordy, ds_add(offsety, ds_set(-0.5)));
                unsigned int iters = escape_iters(cx, cy, lowest_iters);
                lowest_iters = min(lowest_iters, iters);
            }
        }

        imageStore(img, ivec2(gl_GlobalInvocationID.xy), uvec4(int(lowest_iters/float(max_iters) * 255), 0, 0, 255));
    }
}
