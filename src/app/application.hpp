#pragma once

// application.hpp — Application root: owns and wires all subsystems.
//
// Startup sequence (Application::init):
//   1. parse_args          — --hidden, --config flags
//   2. init_directories    — create ~/Library/Application Support/DropAndDrag
//   3. init_logging        — open log file
//   4. init_core_systems   — EventBus, Settings
//   5. init_platform       — NativeWindow, WindowManager, init_native_app()
//   6. init_mouse_shake    — MouseShakeDetector + CGEventTap (non-fatal if denied)
//   7. init_ui             — Renderer, drop/clear/hide callbacks
//   8. wire_event_bus      — (currently no-op; bus is ready but unused)
//   9. create_tray         — SystemTray with quit/toggle actions
//
// Shutdown (Application::shutdown / ~Application):
//   stop_mouse_monitor → cleanup_signal_handlers
//
// Singleton pattern:
//   Application::instance() returns the process-wide singleton.
//   The object is never exposed to untrusted code; callers that need services
//   call the relevant subsystem's own instance() instead.

#include <atomic>
#include <memory>
#include <string>

namespace dd {

// Forward declarations — keep this header fast to include.
class ItemStore;
class Settings;
class EventBus;
class WindowManager;
class NativeWindow;
class MouseShakeDetector;
class Renderer;

class Application {
public:
    // Process-wide singleton.
    static Application& instance();

    Application(const Application&)            = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&)                 = delete;
    Application& operator=(Application&&)      = delete;

    // Run the full startup sequence.  Returns false on fatal error.
    // Non-fatal failures (e.g. Accessibility denied) are logged as warnings.
    bool init(int argc, char* argv[]);

    // Enter the platform run loop.  Blocks until request_shutdown() is called.
    // Returns EXIT_SUCCESS or EXIT_FAILURE.
    int run();

    // Tear down all subsystems.  Idempotent; called by ~Application.
    void shutdown();

    [[nodiscard]] bool is_running() const noexcept {
        return running_.load(std::memory_order_acquire);
    }

    // Safe to call from a signal handler or any thread.
    void request_shutdown() {
        running_.store(false, std::memory_order_release);
    }

private:
    Application();
    ~Application();   // calls shutdown()

    // --- Startup helpers (called by init() in order) ---
    void setup_signal_handlers();
    void cleanup_signal_handlers();
    bool parse_args(int argc, char* argv[]);
    std::string resolve_app_data_dir() const;
    bool init_directories(const std::string& app_data_dir);
    bool init_logging(const std::string& app_data_dir);
    bool init_core_systems();
    bool init_platform();
    bool init_mouse_shake();
    bool init_ui();
    void wire_event_bus();
    void create_tray();

    // --- Platform run-loop implementations (one per OS) ---
    int run_cocoa_loop();   // macOS  — native_loop_step() pumps NSApp
    int run_win32_loop();   // Windows
    int run_linux_loop();   // Linux

    // --- Filesystem / logging paths ---
    std::string app_data_dir_;
    std::string log_path_;

    // --- Command-line flags ---
    bool        start_hidden_{false};   // --hidden: don't show shelf on launch
    std::string config_path_;           // --config PATH: override data directory

    // --- Owned subsystems (construction order = dependency order) ---
    std::unique_ptr<ItemStore>           item_store_;
    std::unique_ptr<Settings>            settings_;
    std::unique_ptr<EventBus>            event_bus_;

    std::unique_ptr<NativeWindow>        native_window_;
    std::unique_ptr<WindowManager>       window_manager_;

    std::unique_ptr<Renderer>            renderer_;

    std::unique_ptr<MouseShakeDetector>  shake_detector_;

    // --- State ---
    std::atomic<bool> running_{false};
    bool              initialized_{false};
};

} // namespace dd
