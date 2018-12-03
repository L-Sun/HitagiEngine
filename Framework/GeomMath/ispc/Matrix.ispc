export void Identity(uniform float mat[],uniform float v, uniform size_t order) {
    foreach(i = 0 ... order*order) {
        mat[i] = i % (order+1) == 0 ? v : 0.0f;
    }
}

void DecreaceByNum(uniform float * varying a, varying float num, uniform size_t count) {
    foreach(index = 0 ... count)
        a[index] = index;
}

export void Inverse(uniform const float mat[], uniform float ret[], uniform size_t order) {
    float* uniform a = uniform new float[order*order];
    foreach (i = 0 ... order*order)
        a[i] = mat[i];

    Identity(ret, 1.0f, order);

    for(int i = 0; i < order; i++) {
        // row operator
        for(int j = i+1; j < order; j++) {
            float x = a[j*order+i]/a[i*(order+1)];
            foreach(k = 0 ... order) {
                a[j*order+k] -= x * a[i*order+k];
                ret[j*order+k] -= x * ret[i*order+k];
            }
        }
    }
    for(int i = 0; i < order; i++) {
        float x = a[i*(order+1)];
        foreach(j = 0 ... order) {
            a[i*order + j] /= x;
            ret[i*order + j] /= x;
        }
    }
    for(int i = order; i >= 0; i--) {
        for(int j = i-1; j >=0; j--) {
            float x = a[j*order+i];
            foreach(k = 0 ... order) {
                a[j*order+k] -= x * a[i*order+k];
                ret[j*order+k] -= x * ret[i*order+k];
            }
        }
    }
}