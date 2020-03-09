export void Max(uniform float res[], uniform const float a[], uniform float b, uniform const size_t count) {
    foreach(index = 0 ... count)
        res[index] = max(a[index], b);

}
export void Min(uniform float res[], uniform const float a[], uniform float b, uniform const size_t count) {
    foreach(index = 0 ... count)
        res[index] = min(a[index], b);
}