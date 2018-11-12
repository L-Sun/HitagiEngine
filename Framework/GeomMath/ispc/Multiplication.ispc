export void MulByElement (uniform const float a[], uniform const float b[], uniform float result[], uniform const size_t count) {
    foreach (index = 0 ... count) {
        result[index] = a[index] * b[index];
    }
}
export void MulByNum (uniform const float a[], uniform const float b, uniform float result[], uniform const size_t count) {
    foreach (index = 0 ... count) {
        result[index] = a[index] * b;
    }
}
export void MulSelfByNum (uniform float a[], uniform const float b, uniform const size_t count) {
    foreach (index = 0 ... count) {
        a[index] *= b;
    }
}
export void DotProduct(uniform const float a[], uniform const float b[], uniform float* uniform result, uniform const size_t count) {
    *result = 0;
    uniform float * uniform r = uniform new uniform float[count];
    MulByElement(a, b, r, count);
    foreach_active(i) {
        *result += r[i];
    }
    delete[] r;
}
export void CrossProduct(uniform const float a[3], uniform const float b[3], uniform float result[3]) {
    foreach (index = 0 ... 3) {
        int index_a   = ((index + 1 == 3) ? 0 : index + 1);
        int index_b   = ((index == 0) ? 2 : index - 1);
        result[index] = a[index_a] * b[index_b] - a[index_b] * b[index_a];
    }
}