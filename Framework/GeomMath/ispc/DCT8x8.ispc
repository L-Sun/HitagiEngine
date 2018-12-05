uniform const float one_over_four = 1.0f / 4.0f;
uniform const float PI_over_sixteen = PI / 16.0f;

inline float normalizing_scale_factor(float a) {
    return a == 0? 1.0f / sqrt(2.0f) : 1.0f;
}

export void DCT8x8(uniform const float in_matrix[64], uniform float out_matrix[64]) {
    uniform float result_cache[8][8][8][8];
    foreach(u = 0 ... 8, v = 0 ... 8, x = 0 ... 8, y = 0 ... 8) {
        float scale_factor = one_over_four * normalizing_scale_factor(u) * normalizing_scale_factor(v);
        float normal = in_matrix[x*8+y] * cos((2.0f*x + 1.0f) * u * PI_over_sixteen) * cos((2.0f * y + 1.0f) * v * PI_over_sixteen);
        result_cache[u][v][x][y] = scale_factor * normal;
    }
    foreach(u = 0 ... 8, v = 0 ... 8) {
        out_matrix[u*8+v] = 0;
        for (int x = 0; x < 8; x++) {
            for (int y = 0; y < 8; y++) {
                out_matrix[u*8+v] += result_cache[u][v][x][y];
            }
        }
    }
}