export void DotProduct(uniform const float a[], uniform const float b[], uniform float* uniform result, uniform size_t count) {
    *result = 0;
    varying float sum = 0.0;
    foreach(index = 0 ... count) {
        sum += a[index] * b[index];
    }
    *result = reduce_add(sum);
}
export void CrossProduct(uniform const float a[3], uniform const float b[3], uniform float result[3]) {
    foreach (index = 0 ... 3) {
        int index_a   = ((index + 1 == 3) ? 0 : index + 1);
        int index_b   = ((index == 0) ? 2 : index - 1);
        result[index] = a[index_a] * b[index_b] - a[index_b] * b[index_a];
    }
}
export void Normalize(uniform float a[], uniform size_t count) {
    uniform float x;
    DotProduct(a, a, &x, count);
    x = sqrt(x);
    foreach(index = 0 ... count)
        a[index] /= x;
}