export void SubByElement(uniform const float a[], uniform const float b[], uniform float result[], uniform size_t count) {
    foreach (index = 0 ... count) {
        result[index] = a[index] - b[index];
    }
}
export void DecreaceByElement(uniform float target[], uniform const float src[], uniform size_t count) {
    foreach (index = 0 ... count) {
        target[index] -= src[index];
    }
}
export void SubByNum(uniform const float a[], uniform const float num, uniform float result[], uniform size_t count) {
    foreach (index = 0 ... count) {
        result[index] = a[index] - num;
    }
}
export void NegateSubByNum(uniform const float num, uniform const float a[], uniform float result[], uniform size_t count) {
        foreach (index = 0 ... count) {
        result[index] = num - a[index];
    }
}
export void DecreaceByNum(uniform float target[], uniform const float num, uniform size_t count) {
    foreach (index = 0 ... count) {
        target[index] -= num;
    }
}
export void Negate(uniform const float src[], uniform float result[], uniform size_t count) {
    foreach (index = 0 ... count) {
        result[index] = -src[index];
    }
}