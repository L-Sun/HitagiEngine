#include <iostream>
#include "geommath.hpp"

using namespace std;
using namespace My;

void vector_test() {
    vec2 x = {55.3f, 22.1f};
    cout << "Vector: " << x << endl;

    vec3 a = {1.0f, 2.0f, 3.0f};
    vec3 b = {5.0f, 6.0f, 7.0f};
    cout << "Vecotr a: " << a << endl;
    cout << "Vecotr b: " << b << endl;

    vec3 c = a + b;
    cout << "a + b = " << c << endl;
}

int main(int argc, char const *argv[]) {
    cout << std::fixed;
    vector_test();
    return 0;
}
