#pragma once

#include <vendor/nlohmann/json.hpp>

#include <shared_mutex>
#include <string>

namespace dd {

struct SettingsData {
    std::string theme{"auto"};
    bool auto_hide{false};
    int auto_hide_delay_ms{500};
    float transparency{1.0f};
    bool always_on_top{false};
    std::string collection_name{"Default"};
    int history_retention_hours{-1};
    std::string global_hotkey{"Ctrl+Shift+D"};
    int shelf_position_x{100};
    int shelf_position_y{100};
    int shelf_position_width{400};
    int shelf_position_height{300};
    bool startup_launch{true};
    bool enable_shake_to_open{true};
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
    SettingsData,
    theme,
    auto_hide,
    auto_hide_delay_ms,
    transparency,
    always_on_top,
    collection_name,
    history_retention_hours,
    global_hotkey,
    shelf_position_x,
    shelf_position_y,
    shelf_position_width,
    shelf_position_height,
    startup_launch,
    enable_shake_to_open
)

class Settings {
public:
    static Settings& instance();

    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;
    Settings(Settings&&) = delete;
    Settings& operator=(Settings&&) = delete;

    [[nodiscard]] SettingsData get() const;
    void set(const SettingsData& data);
    void load(std::string_view path);
    void save(std::string_view path) const;

    [[nodiscard]] std::string theme() const;
    void set_theme(std::string_view value);
    [[nodiscard]] bool auto_hide() const;
    void set_auto_hide(bool value);
    [[nodiscard]] int auto_hide_delay_ms() const;
    void set_auto_hide_delay_ms(int value);
    [[nodiscard]] float transparency() const;
    void set_transparency(float value);
    [[nodiscard]] bool always_on_top() const;
    void set_always_on_top(bool value);
    [[nodiscard]] std::string collection_name() const;
    void set_collection_name(std::string_view value);
    [[nodiscard]] int history_retention_hours() const;
    void set_history_retention_hours(int value);
    [[nodiscard]] std::string global_hotkey() const;
    void set_global_hotkey(std::string_view value);
    [[nodiscard]] int shelf_position_x() const;
    void set_shelf_position_x(int value);
    [[nodiscard]] int shelf_position_y() const;
    void set_shelf_position_y(int value);
    [[nodiscard]] int shelf_position_width() const;
    void set_shelf_position_width(int value);
    [[nodiscard]] int shelf_position_height() const;
    void set_shelf_position_height(int value);
    [[nodiscard]] bool startup_launch() const;
    void set_startup_launch(bool value);
    [[nodiscard]] bool enable_shake_to_open() const;
    void set_enable_shake_to_open(bool value);

private:
    Settings() = default;

    mutable std::shared_mutex mutex_;
    SettingsData data_;
    std::string current_path_;
};

} // namespace dd
