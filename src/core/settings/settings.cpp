#include "settings.hpp"

#include <fstream>

namespace dd {

Settings& Settings::instance() {
    static Settings settings;
    return settings;
}

SettingsData Settings::get() const {
    std::shared_lock lock(mutex_);
    return data_;
}

void Settings::set(const SettingsData& data) {
    {
        std::unique_lock lock(mutex_);
        data_ = data;
    }
    if (!current_path_.empty()) save(current_path_);
}

void Settings::load(std::string_view path) {
    std::ifstream file(std::string(path));
    if (!file.is_open()) return;

    nlohmann::json j;
    file >> j;

    std::unique_lock lock(mutex_);
    data_ = j.get<SettingsData>();
    current_path_ = path;
}

void Settings::save(std::string_view path) const {
    std::shared_lock lock(mutex_);
    nlohmann::json j = data_;

    std::ofstream file(std::string(path));
    file << j.dump(2);
}

std::string Settings::theme() const { std::shared_lock lock(mutex_); return data_.theme; }
void Settings::set_theme(std::string_view value) { std::unique_lock lock(mutex_); data_.theme = value; }

bool Settings::auto_hide() const { std::shared_lock lock(mutex_); return data_.auto_hide; }
void Settings::set_auto_hide(bool value) { std::unique_lock lock(mutex_); data_.auto_hide = value; }

int Settings::auto_hide_delay_ms() const { std::shared_lock lock(mutex_); return data_.auto_hide_delay_ms; }
void Settings::set_auto_hide_delay_ms(int value) { std::unique_lock lock(mutex_); data_.auto_hide_delay_ms = value; }

float Settings::transparency() const { std::shared_lock lock(mutex_); return data_.transparency; }
void Settings::set_transparency(float value) { std::unique_lock lock(mutex_); data_.transparency = std::clamp(value, 0.0f, 1.0f); }

bool Settings::always_on_top() const { std::shared_lock lock(mutex_); return data_.always_on_top; }
void Settings::set_always_on_top(bool value) { std::unique_lock lock(mutex_); data_.always_on_top = value; }

std::string Settings::collection_name() const { std::shared_lock lock(mutex_); return data_.collection_name; }
void Settings::set_collection_name(std::string_view value) { std::unique_lock lock(mutex_); data_.collection_name = value; }

int Settings::history_retention_hours() const { std::shared_lock lock(mutex_); return data_.history_retention_hours; }
void Settings::set_history_retention_hours(int value) { std::unique_lock lock(mutex_); data_.history_retention_hours = value; }

std::string Settings::global_hotkey() const { std::shared_lock lock(mutex_); return data_.global_hotkey; }
void Settings::set_global_hotkey(std::string_view value) { std::unique_lock lock(mutex_); data_.global_hotkey = value; }

int Settings::shelf_position_x() const { std::shared_lock lock(mutex_); return data_.shelf_position_x; }
void Settings::set_shelf_position_x(int value) { std::unique_lock lock(mutex_); data_.shelf_position_x = value; }

int Settings::shelf_position_y() const { std::shared_lock lock(mutex_); return data_.shelf_position_y; }
void Settings::set_shelf_position_y(int value) { std::unique_lock lock(mutex_); data_.shelf_position_y = value; }

int Settings::shelf_position_width() const { std::shared_lock lock(mutex_); return data_.shelf_position_width; }
void Settings::set_shelf_position_width(int value) { std::unique_lock lock(mutex_); data_.shelf_position_width = value; }

int Settings::shelf_position_height() const { std::shared_lock lock(mutex_); return data_.shelf_position_height; }
void Settings::set_shelf_position_height(int value) { std::unique_lock lock(mutex_); data_.shelf_position_height = value; }

bool Settings::startup_launch() const { std::shared_lock lock(mutex_); return data_.startup_launch; }
void Settings::set_startup_launch(bool value) { std::unique_lock lock(mutex_); data_.startup_launch = value; }

} // namespace dd
