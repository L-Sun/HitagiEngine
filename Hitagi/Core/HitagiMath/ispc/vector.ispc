//-----------------
// Float
//------------------

export void zero_float (uniform float data[], uniform int size) {
    foreach (i = 0 ... size)
        data[i] = 0.0f;
}

export void vector_inverse_float(const uniform float data[], uniform float out[], const uniform int size) {
    foreach(i = 0 ... size)    
        out[i] = -data[i];
}

export void vector_add_assgin_float (uniform float a[], const uniform float b[], const uniform int size) {
    foreach (i = 0 ... size)
        a[i] += b[i];
}

export void  vector_add_float(const uniform float a[], const uniform float b[], uniform float out[], const uniform int size) {
    foreach (i = 0 ... size)
        out[i] = a[i] + b[i];
}

export void vector_sub_assgin_float(uniform float a[], const uniform float b[], const uniform int size) {
    foreach (i = 0 ... size)
        a[i] -= b[i];
}

export void vector_sub_float (const uniform float a[], const uniform float b[], uniform float out[], const uniform int size) {
    foreach (i = 0 ... size)
        out[i] = a[i] - b[i];
}

export void vector_mult_assgin_float (uniform float a[], const uniform float b, const uniform int size) {
    foreach (i = 0 ... size)
        a[i] *= b;
}

export void vector_mult_float (const uniform float a[], const uniform float b, uniform float out[], const uniform int size) {
    foreach (i = 0 ... size)
        out[i] = a[i] * b;
}

export void vector_mult_vector_float (const uniform float a[], const uniform float b[], uniform float out[], const uniform int size) {
    foreach (i = 0 ... size)
        out[i] = a[i] * b[i];
}

export void vector_div_assign_float (uniform float a[], const uniform float b, const uniform int size) {
    foreach (i = 0 ... size)
        a[i] /= b;
}

export void vector_div_float (const uniform float a[], const uniform float b, uniform float out[], const uniform int size) {
    foreach (i = 0 ... size)
        out[i] = a[i] / b;
}

export uniform float vector_dot_float (const uniform float a[], const uniform float b[], const uniform int size) {
    varying float sum = 0.0f;
    foreach (i = 0 ... size)
        sum += a[i] * b[i];
    return reduce_add(sum);
}

//-----------------
// Double
//------------------
export void zero_double (uniform double data[], uniform int size) {
    foreach (i = 0 ... size)
        data[i] = 0.0f;
}

export void vector_inverse_double(const uniform double data[], uniform double out[], const uniform int size) {
    foreach(i = 0 ... size)    
        out[i] = -data[i];
}

export void vector_add_assgin_double (uniform double a[], const uniform double b[], const uniform int size) {
    foreach (i = 0 ... size)
        a[i] += b[i];
}

export void  vector_add_double(const uniform double a[], const uniform double b[], uniform double out[], const uniform int size) {
    foreach (i = 0 ... size)
        out[i] = a[i] + b[i];
}

export void vector_sub_assgin_double(uniform double a[], const uniform double b[], const uniform int size) {
    foreach (i = 0 ... size)
        a[i] -= b[i];
}

export void vector_sub_double (const uniform double a[], const uniform double b[], uniform double out[], const uniform int size) {
    foreach (i = 0 ... size)
        out[i] = a[i] - b[i];
}

export void vector_mult_assgin_double (uniform double a[], const uniform double b, const uniform int size) {
    foreach (i = 0 ... size)
        a[i] *= b;
}

export void vector_mult_double (const uniform double a[], const uniform double b, uniform double out[], const uniform int size) {
    foreach (i = 0 ... size)
        out[i] = a[i] * b;
}

export void vector_mult_vector_double (const uniform double a[], const uniform double b[], uniform double out[], const uniform int size) {
    foreach (i = 0 ... size)
        out[i] = a[i] * b[i];
}

export void vector_div_assign_double (uniform double a[], const uniform double b, const uniform int size) {
    foreach (i = 0 ... size)
        a[i] /= b;
}

export void vector_div_double (const uniform double a[], const uniform double b, uniform double out[], const uniform int size) {
    foreach (i = 0 ... size)
        out[i] = a[i] / b;
}

export uniform double vector_dot_double (const uniform double a[], const uniform double b[], const uniform int size) {
    varying double sum = 0.0f;
    foreach (i = 0 ... size)
        sum += a[i] * b[i];
    return reduce_add(sum);
}

