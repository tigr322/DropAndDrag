#pragma once

// native_app.hpp — Platform run-loop helpers.
// init_native_app(): one-time platform setup (NSApp activation, Win32 init, etc.).
// native_loop_step(): pumps one event-loop iteration (called by Application::run).


namespace dd {

void init_native_app();
void run_native_loop();
void terminate_native_app();
bool native_loop_step();

} // namespace dd
