#ifndef PIPELINE_HPP
#define PIPELINE_HPP

#include "equalizer.hpp"
#include "logger.hpp"
#include "pa_manager.hpp"
#include <gio/gio.h>
#include <gst/gst.h>
#include <memory>
#include <sigc++/sigc++.h>

class Pipeline {
  public:
    Pipeline(PAManager* pamanager);
    ~Pipeline();

    logging::EqnixLogger logger = logging::EqnixLogger::create("Pipeline");

    bool playing = false;
    GstClockTime state_check_timeout = 5 * GST_SECOND;

    PAManager* pam = nullptr;

    GstElement *pipeline = nullptr, *source = nullptr, *converter = nullptr, *sink = nullptr;
    GstBus* bus = nullptr;

    std::unique_ptr<Equalizer> equalizer;

    void set_source_monitor_name(const std::string& name);
    void set_output_sink_name(const std::string& name);
    void set_null_pipeline();
    void update_pipeline_state();

  private:
    std::vector<std::shared_ptr<AppInfo>> apps_list;
    uint current_rate = 0;
    GstElement* capsfilter = nullptr;

    GstElement* ensure_factory_create(std::string factory, std::string name);

    void set_pulseaudio_props(const std::string& props);
    void set_caps(const uint& sampling_rate);
    void on_app_added(const std::shared_ptr<AppInfo>& app_info);
    void on_app_changed(const std::shared_ptr<AppInfo>& app_info);
    void on_app_removed(uint idx);
    void on_sink_changed(const std::shared_ptr<SinkInfo>& sink_info);
    void on_source_changed(const std::shared_ptr<SourceInfo>& source_info);

    auto apps_want_to_play() -> bool;
};

#endif // PIPELINE_HPP