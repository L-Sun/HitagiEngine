export void Identity(uniform float mat[], uniform float v, uniform size_t order) {
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

export uniform bool Inverse(uniform const float mat[], uniform float ret[], uniform size_t n) {
    float* uniform a = uniform new float[n*n];

    foreach (i = 0 ... n*n)
        a[i] = mat[i];

    Identity(ret, 1.0f, n);

    for (int i = 0; i < n; i++) {
        // find principal element
        int k = i;
        for (int j = i + 1; j < n; j++)
            if (abs(a[j*n + i]) > abs(a[i*n + i]))
                k = j;

        if (abs(a[k*n + i]) < 1E-10) {
            return false;
        }
        // if principal element is not in row i, swap row i and row k
        if (k != i) {
            foreach (j = 0 ... n) {
                float temp = a[i*n + j];
                a[i*n +j] = a[k*n +j];
                a[k*n +j] = temp;
                temp = ret[i*n +j];
                ret[i*n +j] = ret[k*n +j];
                ret[k*n +j] = temp;
            }
        }

        float d = 1/a[i*n + i];
        for(int j = i + 1; j < n; j++) {
            float alpha = a[j*n + i] * d;
            foreach(k = 0 ... n) {
                a[j*n + k] -= alpha * a[i*n + k];
                ret[j*n + k] -= alpha * ret[i*n + k];
            }
        }
    }

    foreach(i = 0 ... n) {
        float d = 1/a[i*n + i];
        for(int j = 0; j < n; j++) {
            a[i*n + j] *= d;
            ret[i*n + j] *= d;
        }
    }

    for(int i = n-1; i >= 0; i--) {
        for(int j = i-1; j >=0; j--) {
            float alpha = a[j*n+i];
            foreach(k = 0 ... n) {
                a[j*n + k] -= alpha * a[i*n+k];
                ret[j*n + k] -= alpha * ret[i*n+k];
            }
        }
    }

    return true;

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
