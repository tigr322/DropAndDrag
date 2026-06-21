// native_app_linux.cpp — Linux implementation of the native app lifecycle hooks.

#include "platform/native_app.hpp"

namespace dd {

void init_native_app() {}

bool native_loop_step() {
    return true;
}

void run_native_loop() {}

void terminate_native_app() {}

} // namespace dd
