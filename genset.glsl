#version 460 core
layout(local_size_x = 30, local_size_y = 30, local_size_z = 1) in;

layout (r8ui, binding = 0) uniform uimage2D img;
uniform dvec2 bottom_left;
uniform dvec2 upper_right;

layout(std430, binding = 1) buffer layoutName
{
    double debug_data;
};

uniform float max_iters;
uniform dvec2 mag;
uniform dvec2 offsetx;
uniform dvec2 offsety;

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

precise dvec2 ds_mul (precise dvec2 dsa, precise dvec2 dsb)
{
    precise dvec2 dsc;
    precise double c11, c21, c2, e, t1, t2;
    precise double a1, a2, b1, b2, cona, conb, split = 8193.;

    cona = dsa.x * split;
    conb = dsb.x * split;
    a1 = cona - (cona - dsa.x);
    b1 = conb - (conb - dsb.x);
    a2 = dsa.x - a1;
    b2 = dsb.x - b1;

    c11 = dsa.x * dsb.x;
    c21 = a2 * b2 + (a2 * b1 + (a1 * b2 + (a1 * b1 - c11)));

    c2 = dsa.x * dsb.y + dsa.y * dsb.x;

    t1 = c11 + c2;
    e = t1 - c11;
    t2 = dsa.y * dsb.y + ((c2 - e) + (c11 - (t1 - e))) + c21;

    dsc.x = t1 + t2;
    dsc.y = t2 - (dsc.x - t1);

    return dsc;
}

dvec2 Split64(double d)
{
    const double SPLITTER = (1 << 29) + 1;
    double t = d * SPLITTER;
    dvec2 result;
    result.x = t - (t - d);
    result.y = d - result.x;

    return result;
}

precise dvec2 twoProd(precise double a, precise double b) {
    dvec2 p;
    p.x = a * b;
    dvec2 aS = Split64(a);
    dvec2 bS = Split64(b);
    p.y = (aS.x * bS.x - p.x) + aS.x * bS.y + aS.y * bS.x + aS.y * bS.y;
    return p;
}

precise dvec2 ds_div(precise dvec2 b, precise dvec2 a) {
    precise double xn = 1.0 / a.x;
    precise double yn = b.x * xn;
    precise double diff = (ds_add(b, -ds_mul(a, dvec2(yn, 0.0)))).x;
    precise dvec2 prod = twoProd(xn, diff);
    return ds_add(dvec2(yn, 0.0), prod);
}

//precise unsigned int escape_iters(dvec2 c) {
    //dvec2 z = dvec2(0.0, 0.0);
    //dvec2 z_sq = dvec2(0.0, 0.0);
//
    //unsigned int i;
    //for (i = 0; i < max_iters; i++) {
        //z = dvec2(z_sq.x - z_sq.y, 2 * z.x * z.y) + c;
//
        //z_sq = z * z;
        //if (z_sq.x + z_sq.y > 4.0f)
            //return i;
    //}
    //return i;
//}


precise unsigned int escape_iters(dvec2 cx, dvec2 cy) {
    dvec2 zx = dvec2(0.0, 0.0);
    dvec2 zy = dvec2(0.0, 0.0);
    dvec2 z_sqx = dvec2(0.0, 0.0);
    dvec2 z_sqy = dvec2(0.0, 0.0);

    unsigned int i;
    for (i = 0; i < max_iters; i++) {
        zy = ds_add(ds_mul(ds_mul(ds_set(2.0), zx), zy), cy);
        zx = ds_add(ds_add(z_sqx, -z_sqy), cx);

        z_sqx = ds_mul(zx, zx);
        z_sqy = ds_mul(zy, zy);
        if (ds_add(z_sqx, z_sqy).x > 4.0)
            return i;
    }
    return i;
}

precise void main() {
    dvec2 new_coordx = ds_add(ds_div(ds_set(double(gl_GlobalInvocationID.x)/imageSize(img).x - 0.5), mag), ds_set(0.5));
    dvec2 new_coordy = ds_add(ds_div(ds_set(double(gl_GlobalInvocationID.y)/imageSize(img).y - 0.5), mag), ds_set(0.5));
    //dvec2 cx = ds_add(ds_add(ds_mul(new_coordx , ds_set(upper_right.x) - bottom_left.x) , ds_set(bottom_left.x)) , offsetx);
    //dvec2 cy = ds_add(ds_add(ds_mul(new_coordy , ds_set(upper_right.y) - bottom_left.y) , ds_set(bottom_left.y)) , offsety);
    dvec2 cx = ds_add(new_coordx, ds_add(offsetx, ds_set(-0.5)));
    dvec2 cy = ds_add(new_coordy, ds_add(offsety, ds_set(-0.5)));
    unsigned int iters = escape_iters(cx, cy);

    imageStore(img, ivec2(gl_GlobalInvocationID.xy), uvec4(int(iters/float(max_iters) * 255), 0, 0, 255));
}
