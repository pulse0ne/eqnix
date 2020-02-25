#ifndef PA_MANAGER_HPP
#define PA_MANAGER_HPP

#include <glibmm.h>
#include <pulse/pulseaudio.h>
#include <sigc++/sigc++.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <memory>
#include "logger.hpp"

struct ServerInfo {
    std::string server_name;
    std::string server_version;
    std::string default_sink_name;
    std::string default_source_name;
    std::string protocol;
    std::string format;
    std::string channel_map;
    uint rate;
    uint8_t channels;
};

struct SinkInfo {
    std::string name;
    uint index;
    std::string description;
    uint owner_module;
    uint monitor_source;
    std::string monitor_source_name;
    uint rate;
    std::string format;
    std::string active_port;
};

struct SourceInfo {
    std::string name;
    uint index;
    std::string description;
    uint rate;
    std::string format;
    std::string active_port;
};

struct ModuleInfo {
    std::string name;
    uint index;
    std::string argument;
};

struct ClientInfo {
    std::string name;
    uint index;
    std::string binary;
};

struct AppInfo {
    std::string app_type;
    uint index;
    std::string name;
    std::string icon_name;
    uint8_t channels;
    float volume;
    uint rate;
    std::string resampler;
    std::string format;
    int mute;
    bool connected;
    uint buffer;
    uint latency;
    int corked;
    bool wants_to_play;
};

class PAManager {
public:
    PAManager();
    PAManager(const PAManager&) = delete;
    auto operator=(const PAManager&) -> PAManager& = delete;
    PAManager(const PAManager&&) = delete;
    auto operator=(const PAManager&&) -> PAManager& = delete;
    ~PAManager();

    logging::EqnixLogger logger = logging::EqnixLogger("PAManager");

    pa_threaded_mainloop* main_loop = nullptr;

    ServerInfo server_info;
    std::shared_ptr<SinkInfo> apps_sink_info;
    // std::shared_ptr<SinkInfo> mic_sink_info;

    std::vector<std::string> blacklist_in;
    std::vector<std::string> blacklist_out;

    auto get_sink_info(const std::string& name) -> std::shared_ptr<SinkInfo>;
    auto get_source_info(const std::string& name) -> std::shared_ptr<SourceInfo>;

    void find_sink_inputs();
    void find_source_outputs();
    void find_sinks();
    void find_sources();
    void move_sink_input_to_eqnix(const std::string& name, uint idx);
    void remove_sink_input_from_eqnix(const std::string& name, uint idx);
    void move_source_output_to_eqnix(const std::string& name, uint idx);
    void remove_source_output_from_eqnix(const std::string& name, uint idx);
    void set_sink_input_volume(const std::string& name, uint idx, uint8_t channels, uint value);
    void set_sink_input_mute(const std::string& name, uint idx, bool state);
    void set_source_output_volume(const std::string& name, uint idx, uint8_t channels, uint value);
    void set_source_output_mute(const std::string& name, uint idx, bool state);
    void get_sink_input_info(uint idx);
    void update_server_info(const pa_server_info* info);
    void get_modules_info();
    void get_clients_info();

    sigc::signal<void, std::shared_ptr<SourceInfo>> source_added;
    sigc::signal<void, std::shared_ptr<SourceInfo>> source_changed;
    sigc::signal<void, uint> source_removed;
    sigc::signal<void, std::shared_ptr<SinkInfo>> sink_added;
    sigc::signal<void, std::shared_ptr<SinkInfo>> sink_changed;
    sigc::signal<void, uint> sink_removed;
    sigc::signal<void, std::string> new_default_sink;
    sigc::signal<void, std::string> new_default_source;
    sigc::signal<void, std::shared_ptr<AppInfo>> sink_input_added;
    sigc::signal<void, std::shared_ptr<AppInfo>> sink_input_changed;
    sigc::signal<void, uint> sink_input_removed;
    sigc::signal<void, std::shared_ptr<AppInfo>> source_output_added;
    sigc::signal<void, std::shared_ptr<AppInfo>> source_output_changed;
    sigc::signal<void, uint> source_output_removed;
    sigc::signal<void> server_changed;
    sigc::signal<void, std::shared_ptr<ModuleInfo>> module_info;
    sigc::signal<void, std::shared_ptr<ClientInfo>> client_info;

private:
    bool context_ready = false;

    pa_mainloop_api* main_loop_api = nullptr;
    pa_context* context = nullptr;

    std::array<std::string, 6> blacklist_apps = {"gsd-media-keys", "GNOME Shell", "libcanberra", "Screenshot", "speech-dispatcher"};
    std::array<std::string, 4> blacklist_media_name = {"pulsesink probe", "bell-window-system", "audio-volume-change", "screen-capture"};
    std::array<std::string, 1> blacklist_media_role = {"event"};
    std::array<std::string, 4> blacklist_app_id = {"com.github.pulse0ne.eqnix.sinkinputs", "com.github.pulse0ne.eqnix.sourceoutputs", "org.PulseAudio.pavucontrol", "org.gnome.VolumeControl"};

    static void context_state_cb(pa_context* ctx, void* data);
    void subscribe_to_events();
    void get_server_info();
    auto get_default_sink_info() -> std::shared_ptr<SinkInfo>;
    auto get_default_source_info() -> std::shared_ptr<SourceInfo>;
    auto load_sink(const std::string& name, const std::string& description, uint rate) -> std::shared_ptr<SinkInfo>;
    void load_apps_sink();
    // void load_mic_sink();
    auto load_module(const std::string& name, const std::string& argument) -> bool;
    void unload_module(uint idx);
    void unload_sinks();
    void drain_context();
    void new_app(const pa_sink_input_info* info);
    void new_app(const pa_source_output_info* info);
    void changed_app(const pa_sink_input_info* info);
    void changed_app(const pa_source_output_info* info);
    static void print_app_info(const std::shared_ptr<AppInfo>& info);
    auto app_is_connected(const pa_sink_input_info* info) -> bool;
    auto app_is_connected(const pa_source_output_info* info) -> bool;
    static auto get_latency(const pa_sink_input_info* info) -> uint { return info->sink_usec; }
    static auto get_latency(const pa_source_output_info* info) -> uint { return info->source_usec; }

    template <typename T>
    auto parse_app_info(const T& info) -> std::shared_ptr<AppInfo> {
        std::string app_name;
        std::string media_name;
        std::string media_role;
        std::string app_id;
        std::string icon_name;
        auto ai = std::make_shared<AppInfo>();
        bool forbidden_app = false;
        auto prop = pa_proplist_gets(info->proplist, "application.name");
        if (prop != nullptr) {
            app_name = prop;
            logging::defaultLogger.debug("parse_app_info: " + app_name);
            forbidden_app = std::find(std::begin(blacklist_apps), std::end(blacklist_apps), app_name) != std::end(blacklist_apps);
            if (forbidden_app) {
                return nullptr;
            }
        }
        prop = pa_proplist_gets(info->proplist, "media.name");
        if (prop != nullptr) {
            media_name = prop;
            if (app_name.empty()) {
                app_name = media_name;
            }
            forbidden_app = std::find(std::begin(blacklist_media_name), std::end(blacklist_media_name), media_name) != std::end(blacklist_media_name);
            if (forbidden_app) {
                return nullptr;
            }
        }
        prop = pa_proplist_gets(info->proplist, "media.role");
        if (prop != nullptr) {
            media_role = prop;
            forbidden_app = std::find(std::begin(blacklist_media_role), std::end(blacklist_media_role), media_role) != std::end(blacklist_media_role);
            if (forbidden_app) {
                return nullptr;
            }
        }
        prop = pa_proplist_gets(info->proplist, "application.id");
        if (prop != nullptr) {
            app_id = prop;
            forbidden_app = std::find(std::begin(blacklist_app_id), std::end(blacklist_app_id), app_id) != std::end(blacklist_app_id);
            if (forbidden_app) {
                return nullptr;
            }
        }
        prop = pa_proplist_gets(info->proplist, "application.icon_name");
        if (prop != nullptr) {
            icon_name = prop;
        } else {
            prop = pa_proplist_gets(info->proplist, "media.icon_name");
            if (prop != nullptr) {
                if (std::strcmp(prop, "audio-card-bluetooth") == 0) {  // there is no GTK icon with this name given by Pulseaudio =/
                } else {
                    icon_name = "bluetooth-symbolic";
                }
            } else {
                icon_name = "audio-x-generic-symbolic";
            }
        }

        if (info->resample_method) {
            ai->resampler = info->resample_method;
        } else {
            ai->resampler = "none";
        }

        ai->connected = app_is_connected(info);
        ai->volume = 100 * pa_cvolume_max(&info->volume) / PA_VOLUME_NORM;
        ai->format = pa_sample_format_to_string(info->sample_spec.format);
        ai->index = info->index;
        ai->name = app_name;
        ai->icon_name = icon_name;
        ai->channels = info->volume.channels;
        ai->rate = info->sample_spec.rate;
        ai->mute = info->mute;
        ai->buffer = info->buffer_usec;
        ai->latency = get_latency(info);
        ai->corked = info->corked;
        ai->wants_to_play = ai->connected && !ai->corked;

        return ai;
    }
};

#endif // PA_MANAGER_HPP