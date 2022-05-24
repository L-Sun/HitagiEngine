#include <iostream>
#include <hitagi/application.hpp>

using namespace hitagi;

auto main(int argc, char** argv) -> int {
    int ret = 0;

    g_App->SetCommandLineParameters(argc, argv);

    if ((ret = g_App->Initialize()) != 0) {
        std::cout << "App Initialize failed, will exit now." << std::endl;
        return ret;
    }

    while (!g_App->IsQuit()) {
        g_App->Tick();
    }

    g_App->Finalize();

    return 0;
}
