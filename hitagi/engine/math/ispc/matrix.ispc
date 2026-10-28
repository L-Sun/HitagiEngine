// Row-major 4x4 matrices. Each lane owns an output element; no horizontal reduction.
// Output must not alias either input (Matrix::operator* returns a fresh matrix).
export void matrix4_mult_float(const uniform float a[], const uniform float b[], uniform float out[]) {
    foreach (i = 0 ... 16) {
        int row = i / 4;
        int col = i % 4;
        float sum = 0.0f;
        for (uniform int k = 0; k < 4; ++k)
            sum += a[row * 4 + k] * b[k * 4 + col];
        out[i] = sum;
    }
}

export void matrix4_mult_double(const uniform double a[], const uniform double b[], uniform double out[]) {
    foreach (i = 0 ... 16) {
        int row = i / 4;
        int col = i % 4;
        double sum = 0.0;
        for (uniform int k = 0; k < 4; ++k)
            sum += a[row * 4 + k] * b[k * 4 + col];
        out[i] = sum;
    }
}

// One host call per batch; the 4D branch uses constant strides and four explicit terms.
export void matrix_mult_batch_float(const uniform float a[], const uniform float b[], uniform float out[],
                                    uniform int dimension, uniform uint64 count, uniform bool broadcast_lhs) {
    if (dimension == 4) {
        for (uniform uint64 m = 0; m < count; ++m) {
            uniform uint64 lhs_base = broadcast_lhs ? 0 : m * 16;
            uniform uint64 base = m * 16;
            foreach (i = 0 ... 16) {
                int row = i / 4;
                int col = i % 4;
                float sum = a[lhs_base + row * 4] * b[base + col];
                sum += a[lhs_base + row * 4 + 1] * b[base + 4 + col];
                sum += a[lhs_base + row * 4 + 2] * b[base + 8 + col];
                sum += a[lhs_base + row * 4 + 3] * b[base + 12 + col];
                out[base + i] = sum;
            }
        }
    } else {
        uniform uint64 size = (uniform uint64)dimension * dimension;
        for (uniform uint64 m = 0; m < count; ++m) {
            uniform uint64 lhs_base = broadcast_lhs ? 0 : m * size;
            uniform uint64 base = m * size;
            for (uniform int row = 0; row < dimension; ++row) {
                foreach (col = 0 ... dimension) {
                    float sum = 0;
                    for (uniform int k = 0; k < dimension; ++k)
                        sum += a[lhs_base + (uniform uint64)row * dimension + k] * b[base + (uniform uint64)k * dimension + col];
                    out[base + (uniform uint64)row * dimension + col] = sum;
                }
            }
        }
    }
}

// One host call per batch; the 4D branch uses constant strides and four explicit terms.
export void matrix_mult_batch_double(const uniform double a[], const uniform double b[], uniform double out[],
                                    uniform int dimension, uniform uint64 count, uniform bool broadcast_lhs) {
    if (dimension == 4) {
        for (uniform uint64 m = 0; m < count; ++m) {
            uniform uint64 lhs_base = broadcast_lhs ? 0 : m * 16;
            uniform uint64 base = m * 16;
            foreach (i = 0 ... 16) {
                int row = i / 4;
                int col = i % 4;
                double sum = a[lhs_base + row * 4] * b[base + col];
                sum += a[lhs_base + row * 4 + 1] * b[base + 4 + col];
                sum += a[lhs_base + row * 4 + 2] * b[base + 8 + col];
                sum += a[lhs_base + row * 4 + 3] * b[base + 12 + col];
                out[base + i] = sum;
            }
        }
    } else {
        uniform uint64 size = (uniform uint64)dimension * dimension;
        for (uniform uint64 m = 0; m < count; ++m) {
            uniform uint64 lhs_base = broadcast_lhs ? 0 : m * size;
            uniform uint64 base = m * size;
            for (uniform int row = 0; row < dimension; ++row) {
                foreach (col = 0 ... dimension) {
                    double sum = 0;
                    for (uniform int k = 0; k < dimension; ++k)
                        sum += a[lhs_base + (uniform uint64)row * dimension + k] * b[base + (uniform uint64)k * dimension + col];
                    out[base + (uniform uint64)row * dimension + col] = sum;
                }
            }
        }
    }
}
