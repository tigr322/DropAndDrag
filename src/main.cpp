// main.cpp — Entry point.  Delegates immediately to Application.
//
// init() runs the full startup sequence (database, platform, UI, mouse monitor).
// run() enters the platform event loop and blocks until shutdown is requested.
// shutdown() is called explicitly so RAII cleanup is logged before process exit.

#include "app/application.hpp"

#include <cstdlib>
#include <iostream>

#if defined(__linux__)
#include <X11/Xlib.h>
#endif

int main(int argc, char* argv[]) {
#if defined(__linux__)
    // Must be the very first Xlib call in the process so that Xlib is
    // safe for concurrent use from the mouse-monitor poll thread and the
    // main event loop.
    XInitThreads();
#endif
    auto& app = dd::Application::instance();

    if (!app.init(argc, argv)) {
        std::cerr << "Failed to initialize DropAndDrag" << std::endl;
        return EXIT_FAILURE;
    }

    int result = app.run();

    app.shutdown();

    return result;
}
