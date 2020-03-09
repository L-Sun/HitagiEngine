export void DivNum(uniform const float a[], uniform const float b, uniform float result[], uniform size_t count) {
    foreach(index = 0 ... count) {
        result[index] = a[index] / b;
    }
}
export void DivSelfByNum(uniform float src[], uniform const float num, uniform size_t count) {
    foreach(index = 0 ... count) {
        src[index] /= num;
    }
}