#pragma once

#include <atomic>
#include <memory>
#include <string>

namespace dd {

class ItemStore;
class CollectionStore;
class TagStore;
class Settings;
class EventBus;
class SearchEngine;
class ThreadPool;
class WindowManager;
class NativeWindow;
class Database;
class MouseShakeDetector;
class Renderer;

class Application {
public:
    static Application& instance();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    bool init(int argc, char* argv[]);
    int run();
    void shutdown();

    [[nodiscard]] bool is_running() const noexcept { return running_.load(std::memory_order_acquire); }
    void request_shutdown() { running_.store(false, std::memory_order_release); }

private:
    Application();
    ~Application();

    void setup_signal_handlers();
    void cleanup_signal_handlers();
    bool parse_args(int argc, char* argv[]);
    std::string resolve_app_data_dir() const;
    bool init_directories(const std::string& app_data_dir);
    bool init_logging(const std::string& app_data_dir);
    bool init_database(const std::string& app_data_dir);
    bool init_core_systems();
    bool init_threading();
    bool init_platform();
    bool init_mouse_shake();
    bool init_ui();
    void wire_event_bus();
    void create_tray();

    int run_cocoa_loop();
    int run_win32_loop();
    int run_linux_loop();

    std::string app_data_dir_;
    std::string log_path_;
    std::string db_path_;
    bool start_hidden_{false};
    std::string config_path_;

    std::unique_ptr<Database> database_;
    std::unique_ptr<ItemStore> item_store_;
    std::unique_ptr<Settings> settings_;
    std::unique_ptr<EventBus> event_bus_;
    std::unique_ptr<SearchEngine> search_engine_;
    std::unique_ptr<ThreadPool> thread_pool_;

    std::unique_ptr<NativeWindow> native_window_;
    std::unique_ptr<WindowManager> window_manager_;

    std::unique_ptr<Renderer> renderer_;

    std::unique_ptr<MouseShakeDetector> shake_detector_;

    std::atomic<bool> running_{false};
    bool initialized_{false};
};

} // namespace dd
