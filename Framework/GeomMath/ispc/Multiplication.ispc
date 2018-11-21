export void MulByElement (uniform const float a[], uniform const float b[], uniform float result[], uniform size_t count) {
    foreach (index = 0 ... count) {
        result[index] = a[index] * b[index];
    }
}
export void MulByNum (uniform const float a[], uniform const float b, uniform float result[], uniform size_t count) {
    foreach (index = 0 ... count) {
        result[index] = a[index] * b;
    }
}
export void MulSelfByNum (uniform float a[], uniform const float b, uniform size_t count) {
    foreach (index = 0 ... count) {
        a[index] *= b;
    }
}
export void MatMul(uniform const float a[], uniform const float b[], uniform float result[], uniform size_t rows, uniform size_t middle, uniform size_t cols) {
    foreach(row = 0 ... rows, col = 0 ... cols) {
        float sum = 0.0;
        for(int i = 0; i < middle; i++)
            sum += a[row * middle + i] * b[col + i * cols];
        result[row*cols + col] = sum;
    }
}