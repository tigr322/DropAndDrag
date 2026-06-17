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
