export void AddByElement(uniform const float a[], uniform const float b[], uniform float result[], uniform const size_t count) {
    foreach (index = 0 ... count) { result[index] = a[index] + b[index]; }
    return;
}
export void AddByNum(uniform const float a[], uniform const float b, uniform float result[], uniform const size_t count) {
    foreach (index = 0 ... count) { result[index] = a[index] + b; }
    return;
}
export void IncreaceByElement(uniform float traget[], uniform const float src[], uniform const size_t count) {
    foreach (index = 0 ... count) { traget[index] += src[index]; }
    return;
}
export void IncreaceByNum(uniform float traget[], uniform const float num, uniform const size_t count) {
    foreach (index = 0 ... count) { traget[index] += num; }
    return;
}