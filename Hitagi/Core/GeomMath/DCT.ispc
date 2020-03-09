uniform const float one_over_four = 1.0f / 4.0f;
uniform const float PI_over_sixteen = PI / 16.0f;

inline float normalizing_scale_factor(int a) {
    return a == 0? 1.0f / sqrt(2.0f) : 1.0f;
}


export void DCT(uniform const float in_matrix[64], uniform float out_matrix[64]) {
    foreach(i = 0 ... 64) 
        out_matrix[i] = 0.0f;
    
    foreach(u = 0 ... 8, v = 0 ... 8, x = 0 ... 8, y = 0 ... 8) {
        float scale_factor = one_over_four * normalizing_scale_factor(u) * normalizing_scale_factor(v);
        float normial = in_matrix[x*8+y] * cos((2.0f*x + 1.0f) * u * PI_over_sixteen) * cos((2.0f * y + 1.0f) * v * PI_over_sixteen);
        out_matrix[u*8+v] += reduce_add(scale_factor * normial);
    }
}

export void IDCT(uniform const float in_matrix[64], uniform float out_matrix[64]) {
    foreach(i = 0 ... 64) 
        out_matrix[i] = 0.0f;

    foreach(x = 0 ... 8, y = 0 ... 8, u = 0 ... 8, v = 0 ... 8) {
        float scale_factor = one_over_four * normalizing_scale_factor(u) * normalizing_scale_factor(v);
        float normial = in_matrix[u*8+v] * cos((2.0f*x + 1.0f) * u * PI_over_sixteen) * cos((2.0f * y + 1.0f) * v * PI_over_sixteen);
        out_matrix[x*8+y] += reduce_add(scale_factor * normial);
    }
}