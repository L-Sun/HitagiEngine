#include "EmptyApplication.hpp"

using namespace Hitagi;

int EmptyApplication::Initialize() {
    BaseApplication::Initialize();
    return 0;
}
void EmptyApplication::Finalize() { BaseApplication::Finalize(); }
void EmptyApplication::Tick() {}