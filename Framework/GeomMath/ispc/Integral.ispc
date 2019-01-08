typedef uniform float (*FPType)(float);

uniform float SumOfItem(uniform float a, uniform unsigned int k, float h, uniform FPType f) {
    varying float sum = 0.0;
    uniform int32 end = (0x1u << (k-1)) - 1;
    foreach(i = 0 ... end) {
        sum += f(a + (2 * i + 1) * h);
    }
    return reduce_add(sum);
}
uniform float T0(const uniform float a, const uniform float b, uniform unsigned int k, uniform FPType f) {
    if (k == 0)
        return 0.5 * (b - a) * (f(a) + f(b));
    else {
        uniform float h = (b - a) / ((float)(0x1u << k));
        return 0.5 * T0(a, b, k-1, f) + h * SumOfItem(a, k, h, f);
    }
}

uniform float T1(const uniform float a, const uniform float b, uniform unsigned int k, uniform FPType f, uniform float t[3]){ 
    uniform float tmp = k == 0 ? T0(a, b, k, f) : t[0];
    t[0] = T0(a, b, k + 1, f);
    return (4.0 * t[0] - tmp) / 3.0; 
};
uniform float T2(const uniform float a, const uniform float b, uniform unsigned int k, uniform FPType f, uniform float t[3]){ 
    uniform float tmp = k == 0 ? T1(a, b, k, f, t) : t[1];
    t[1] = T1(a, b, k + 1, f, t);
    return (16.0 * t[1] - tmp) / 15.0;
};
uniform float T3(const uniform float a, const uniform float b, uniform unsigned int k, uniform FPType f, uniform float t[3]){ 
    uniform float tmp = k == 0 ? T2(a, b, k, f, t) : t[2];
    t[2] = T2(a, b, k + 1, f, t);
    return (64.0 * t[2] - tmp) / 63.0;
};


export uniform float Integral(const uniform float a, const uniform float b, const uniform float precision, uniform FPType f) {
    uniform float t[3];
    uniform float R1 = 0.0, R2 = 1.0;
    uniform unsigned int k = 0;
    while(abs(R2-R1)>precision) {
        R1 = R2;
        R2 = T3(a, b, k, f, t);
        k++;
    }
    return R2;
}