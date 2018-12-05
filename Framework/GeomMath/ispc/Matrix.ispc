export void Identity(uniform float mat[],uniform float v, uniform size_t order) {
    foreach(i = 0 ... order*order) {
        mat[i] = i % (order+1) == 0 ? v : 0.0f;
    }
}

float invf(int i, int j, uniform const float matrix[16])
{
	int pre_i = ((i == 0) ? 3 : i - 1);
	int next_i = ((i + 1 == 4) ? 0 : i + 1);
	int next_next_i = ((i + 2 >= 4) ? i - 2 : i + 2);
	int pre_j = ((j == 0) ? 3 : j - 1);
	int next_j = ((j + 1 == 4) ? 0 : j + 1);
	int next_next_j = ((j + 2 >= 4) ? j - 2 : j + 2);
    int o = i-j < 0? j-i:i-j;

#define e(a, b) matrix[(a)*4 + (b)]
    float inv =
        + e(next_i, next_j)*e(next_next_i, next_next_j)*e(pre_i, pre_j)
        + e(next_i, next_next_j)*e(next_next_i, pre_j)*e(pre_i, next_j)
        + e(next_i, pre_j)*e(next_next_i, next_j)*e(pre_i, next_next_j)
        - e(next_i, next_j)*e(next_next_i, pre_j)*e(pre_i, next_next_j)
        - e(next_i, next_next_j)*e(next_next_i, next_j)*e(pre_i, pre_j)
        - e(next_i, pre_j)*e(next_next_i, next_next_j)*e(pre_i, next_j);

    return (o&0x1)?-inv : inv;
#undef e
}

export uniform bool InverseMatrix4X4f(uniform float matrix[16])
{
    uniform float inv[16];
    uniform double D = 0;

    foreach_tiled(i = 0 ... 4, j = 0 ... 4) {
        inv[j*4+i] = invf(i,j,matrix);
    }

    foreach(k = 0 ... 4) {
        float s = matrix[k] * inv[k*4];
        D = reduce_add(s);
    }

    if (D==0) return false;

    D = 1.0 / D;

    foreach(i = 0 ... 16) {
        matrix[i] = inv[i] * D;
    }

    return true;
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

export void MatrixExchangeYandZ(uniform float data[], const uniform int rows, const uniform int cols)
{
    foreach(row_index = 0 ... rows) {
        int start_index = row_index * cols;
        unsigned int* p = (unsigned int*)(data + start_index + 1);
        unsigned int* q = (unsigned int*)(data + start_index + 2);
        *p ^= *q;
        *q ^= *p;
        *p ^= *q;
    }
}
