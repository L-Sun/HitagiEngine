#include <iostream>
#include "IApplication.hpp"

using namespace std;
using namespace My;

namespace My {
extern IApplication* g_pApp;
}  // namespace My

int main(int argc, char const* argv[]) {
    int ret;

    if ((ret = g_pApp->Initialize()) != 0) {
        cout << "App Initialize failed, will exit now." << endl;
    }

    while (!g_pApp->IsQuit()) {
        g_pApp->Tick();
    }
    g_pApp->Finalize();

    return 0;
}
