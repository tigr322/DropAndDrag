// main.cpp — Entry point.  Delegates immediately to Application.
//
// init() runs the full startup sequence (database, platform, UI, mouse monitor).
// run() enters the platform event loop and blocks until shutdown is requested.
// shutdown() is called explicitly so RAII cleanup is logged before process exit.

#include "app/application.hpp"

#include <cstdlib>
#include <iostream>

int main(int argc, char* argv[]) {
    auto& app = dd::Application::instance();

    if (!app.init(argc, argv)) {
        std::cerr << "Failed to initialize DropAndDrag" << std::endl;
        return EXIT_FAILURE;
    }

    int result = app.run();

    app.shutdown();

    return result;
}
