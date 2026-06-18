#pragma once

namespace dd {

class MouseShakeDetector;

bool start_mouse_monitor(MouseShakeDetector& detector);
void stop_mouse_monitor();
bool is_mouse_monitor_running();
void set_shelf_visible(bool visible);

} // namespace dd
