#include "application.hpp"

#include <core/database/db.hpp>
#include <core/event_bus/event_bus.hpp>
#include <core/items/item_store.hpp>
#include <core/mouse_shake/mouse_shake_detector.hpp>
#include <core/settings/settings.hpp>
#include <core/threading/thread_pool.hpp>
#include <platform/clipboard/clipboard.hpp>
#include <platform/drag_drop/drag_drop.hpp>
#include <platform/fs_monitor/fs_monitor.hpp>
#include <platform/hotkeys/hotkeys.hpp>
#include <platform/mouse_monitor/mouse_monitor.hpp>
#include <platform/notifications/notifications.hpp>
#include <platform/tray/tray.hpp>
#include <platform/window/native_window.hpp>
#include <platform/window/window_manager.hpp>
#include <ui/animations/animation.hpp>
#include <ui/renderer/renderer.hpp>
#include <ui/themes/theme.hpp>

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <thread>
#include <sstream>
#include <string_view>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(_WIN32)
#include <windows.h>
#else
#include <poll.h>
#include <sys/select.h>
#include <unistd.h>
#endif

namespace dd {

namespace {

std::atomic<Application*> g_app_instance{nullptr};
std::mutex g_log_mutex;
std::ofstream g_log_file;
bool g_log_to_console{true};

void log_message(std::string_view level, std::string_view message) {
    std::lock_guard lock(g_log_mutex);
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << "[" << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
        << "] [" << level << "] " << message << "\n";
    if (g_log_file.is_open()) {
        g_log_file << oss.str();
        g_log_file.flush();
    }
    if (g_log_to_console) {
        std::cerr << oss.str();
    }
}

#if defined(__APPLE__)
void signal_handler(int sig) {
    auto* app = g_app_instance.load(std::memory_order_acquire);
    if (app) {
        app->request_shutdown();
    }
}

struct sigaction g_old_sigint;
struct sigaction g_old_sigterm;

void setup_signals() {
    struct sigaction sa{};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, &g_old_sigint);
    sigaction(SIGTERM, &sa, &g_old_sigterm);
}

void cleanup_signals() {
    sigaction(SIGINT, &g_old_sigint, nullptr);
    sigaction(SIGTERM, &g_old_sigterm, nullptr);
}
#elif defined(_WIN32)
static BOOL WINAPI console_handler(DWORD ctrl_type) {
    if (ctrl_type == CTRL_C_EVENT || ctrl_type == CTRL_CLOSE_EVENT) {
        auto* app = g_app_instance.load(std::memory_order_acquire);
        if (app) {
            app->request_shutdown();
            return TRUE;
        }
    }
    return FALSE;
}

void setup_signals() {
    SetConsoleCtrlHandler(console_handler, TRUE);
}

void cleanup_signals() {
    SetConsoleCtrlHandler(console_handler, FALSE);
}
#else
void signal_handler(int sig) {
    auto* app = g_app_instance.load(std::memory_order_acquire);
    if (app) {
        app->request_shutdown();
    }
}

struct sigaction g_old_sigint;
struct sigaction g_old_sigterm;

void setup_signals() {
    struct sigaction sa{};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, &g_old_sigint);
    sigaction(SIGTERM, &sa, &g_old_sigterm);
}

void cleanup_signals() {
    sigaction(SIGINT, &g_old_sigint, nullptr);
    sigaction(SIGTERM, &g_old_sigterm, nullptr);
}
#endif

} // anonymous namespace

Application& Application::instance() {
    static Application app;
    return app;
}

Application::~Application() {
    shutdown();
}

bool Application::init(int argc, char* argv[]) {
    g_app_instance.store(this, std::memory_order_release);

    if (!parse_args(argc, argv)) {
        return false;
    }

    auto app_data = resolve_app_data_dir();
    if (!init_directories(app_data)) {
        return false;
    }

    if (!init_logging(app_data)) {
        return false;
    }

    app_data_dir_ = std::move(app_data);
    db_path_ = app_data_dir_ + "/dropanddrag.db";

    log_message("INFO", "DropAndDrag v1.0.0 starting");

    setup_signal_handlers();

    if (!init_database(app_data_dir_)) {
        log_message("ERROR", "Failed to initialize database");
        return false;
    }

    if (!init_core_systems()) {
        log_message("ERROR", "Failed to initialize core systems");
        return false;
    }

    if (!init_threading()) {
        log_message("ERROR", "Failed to initialize threading");
        return false;
    }

    if (!init_platform()) {
        log_message("ERROR", "Failed to initialize platform");
        return false;
    }

    if (!init_mouse_shake()) {
        log_message("WARN", "Mouse shake detection unavailable (needs accessibility permission)");
    }

    if (!init_ui()) {
        log_message("ERROR", "Failed to initialize UI");
        return false;
    }

    wire_event_bus();
    create_tray();

    running_.store(true, std::memory_order_release);
    initialized_ = true;
    log_message("INFO", "Application initialized successfully");

    return true;
}

int Application::run() {
    if (!initialized_) {
        return EXIT_FAILURE;
    }

#if defined(__APPLE__)
    return run_cocoa_loop();
#elif defined(_WIN32)
    return run_win32_loop();
#else
    return run_linux_loop();
#endif
}

void Application::shutdown() {
    if (!initialized_) {
        return;
    }

    log_message("INFO", "Shutting down");

    stop_mouse_monitor();

    renderer_->shutdown();
    skia_context_->shutdown();

    thread_pool_->shutdown();

    database_->close();

    cleanup_signal_handlers();

    if (g_log_file.is_open()) {
        g_log_file.close();
    }

    initialized_ = false;
}

bool Application::parse_args(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        if (arg == "--hidden") {
            start_hidden_ = true;
        } else if (arg == "--config" && i + 1 < argc) {
            config_path_ = argv[++i];
        }
    }
    return true;
}

std::string Application::resolve_app_data_dir() const {
    if (!config_path_.empty()) {
        return config_path_;
    }

#if defined(__APPLE__)
    const char* home = std::getenv("HOME");
    if (home) {
        return std::string(home) + "/Library/Application Support/DropAndDrag";
    }
#elif defined(_WIN32)
    const char* appdata = std::getenv("APPDATA");
    if (appdata) {
        return std::string(appdata) + "/DropAndDrag";
    }
#else
    const char* xdg = std::getenv("XDG_DATA_HOME");
    if (xdg && *xdg) {
        return std::string(xdg) + "/DropAndDrag";
    }
    const char* home = std::getenv("HOME");
    if (home) {
        return std::string(home) + "/.local/share/DropAndDrag";
    }
#endif

    return "./DropAndDragData";
}

bool Application::init_directories(const std::string& app_data_dir) {
    std::error_code ec;
    std::filesystem::create_directories(app_data_dir, ec);
    return !ec;
}

bool Application::init_logging(const std::string& app_data_dir) {
    log_path_ = app_data_dir + "/dropanddrag.log";
    g_log_file.open(log_path_, std::ios::out | std::ios::app);
    return g_log_file.is_open();
}

bool Application::init_database(const std::string& app_data_dir) {
    database_ = std::make_unique<Database>();
    auto result = database_->init(db_path_);
    result.wait();
    return result.get();
}

bool Application::init_core_systems() {
    event_bus_ = std::make_unique<EventBus>();
    settings_ = std::make_unique<Settings>();

    auto settings_path = app_data_dir_ + "/settings.json";
    settings_->load(settings_path);

    search_engine_ = std::make_unique<SearchEngine>();
    return true;
}

bool Application::init_threading() {
    thread_pool_ = std::make_unique<ThreadPool>();
    return true;
}

bool Application::init_platform() {
    native_window_ = NativeWindow::create(WindowStyle::Normal);
    window_manager_ = std::make_unique<WindowManager>();

    hotkey_manager_ = std::make_unique<HotkeyManager>();

    auto cfg = settings_->get();
    system_tray_ = std::make_unique<SystemTray>();

    return true;
}

bool Application::init_mouse_shake() {
    auto cfg = settings_->get();
    ShakeConfig shake_cfg;
    shake_cfg.enabled = cfg.enable_shake_to_open;

    shake_detector_ = std::make_unique<MouseShakeDetector>(shake_cfg);

    shake_detector_->set_callback([this]() {
        event_bus_->emit(EventType::ShelfShown);
        log_message("INFO", "Mouse shake detected — opening shelf");
    });

    if (!start_mouse_monitor(*shake_detector_)) {
        shake_detector_.reset();
        return false;
    }

    log_message("INFO", "Mouse shake detection active");
    return true;
}

bool Application::init_ui() {
    skia_context_ = std::make_unique<SkiaContext>();
    renderer_ = std::make_unique<Renderer>();
    theme_ = std::make_unique<Theme>();
    animation_manager_ = std::make_unique<AnimationManager>();

    if (!renderer_->init(nullptr)) {
        return false;
    }

    return true;
}

void Application::wire_event_bus() {
    event_bus_->subscribe(EventType::SettingsChanged, [this](const Event& e) {
        auto new_settings = settings_->get();
        hotkey_manager_->register_hotkey(new_settings.global_hotkey);
        Theme::setVariant(new_settings.theme == "dark"  ? ThemeVariant::Dark
                          : new_settings.theme == "light" ? ThemeVariant::Light
                                                          : ThemeVariant::Dark);
    });
}

void Application::create_tray() {
    system_tray_->create("dropanddrag", "DropAndDrag");

    std::vector<MenuItem> menu = {
        MenuItem{.label = "Show/Hide", .action = "toggle", .enabled = true},
        MenuItem{.label = "", .action = "", .enabled = true, .separator = true},
        MenuItem{.label = "Settings", .action = "settings", .enabled = true},
        MenuItem{.label = "", .action = "", .enabled = true, .separator = true},
        MenuItem{.label = "Quit", .action = "quit", .enabled = true},
    };
    system_tray_->setMenu(menu);
    system_tray_->setMenuCallback([this](std::string_view action) {
        if (action == "quit") {
            request_shutdown();
        } else if (action == "toggle") {
            event_bus_->emit(EventType::ShelfShown);
        }
    });

    if (!start_hidden_) {
        system_tray_->show();
    }
}

void Application::setup_signal_handlers() {
    setup_signals();
}

void Application::cleanup_signal_handlers() {
    cleanup_signals();
}

#if defined(__APPLE__)
int Application::run_cocoa_loop() {
    log_message("INFO", "Entering Cocoa run loop");
    while (running_.load(std::memory_order_acquire)) {
        if (renderer_) {
            renderer_->render(16.67f);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    log_message("INFO", "Exiting Cocoa run loop");
    return EXIT_SUCCESS;
}
#endif

#if defined(_WIN32)
int Application::run_win32_loop() {
    log_message("INFO", "Entering Win32 message loop");
    MSG msg{};
    while (running_.load(std::memory_order_acquire)) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                request_shutdown();
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (renderer_) {
            renderer_->render(16.67f);
        }
        if (thread_pool_ && thread_pool_->pending_tasks() == 0) {
            MsgWaitForMultipleObjects(0, nullptr, FALSE, 16, QS_ALLINPUT);
        }
    }
    log_message("INFO", "Exiting Win32 message loop");
    return EXIT_SUCCESS;
}
#endif

#if !defined(__APPLE__) && !defined(_WIN32)
int Application::run_linux_loop() {
    log_message("INFO", "Entering Linux event loop");
    while (running_.load(std::memory_order_acquire)) {
        if (renderer_) {
            renderer_->render(16.67f);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    log_message("INFO", "Exiting Linux event loop");
    return EXIT_SUCCESS;
}
#endif

} // namespace dd
