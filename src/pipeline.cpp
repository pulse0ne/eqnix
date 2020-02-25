#include "pipeline.hpp"

namespace {

void on_message_error(const GstBus* bus, GstMessage* message, Pipeline* p) {
    GError* err;
    gchar* debug;

    gst_message_parse_error(message, &err, &debug);

    p->logger.critical(err->message);
    p->logger.debug(debug);

    p->set_null_pipeline();

    g_error_free(err);
    g_free(debug);
}

void on_stream_status(const GstBus* bus, GstMessage* message, Pipeline* p) {
    // TODO: RT stuff?
}

} // namespace

Pipeline::Pipeline(PAManager* pamanager) : pam(pamanager) {
    gst_init(nullptr, nullptr);

    pipeline = gst_pipeline_new("eqnix-pipeline");
    bus = gst_element_get_bus(pipeline);

    gst_bus_enable_sync_message_emission(bus);
    gst_bus_add_signal_watch(bus);

    g_signal_connect(bus, "message::error", G_CALLBACK(on_message_error), this);
    g_signal_connect(bus, "sync-message::stream-status", G_CALLBACK(on_stream_status), this);

    equalizer = std::make_unique<Equalizer>();

    source = ensure_factory_create("pulsesrc", "source");
    // capsfilter = ensure_factory_create("capsfilter", nullptr);
    converter = ensure_factory_create("audioconvert", "conv");
    sink = ensure_factory_create("pulsesink", "sink");

    gst_bin_add_many(GST_BIN(pipeline), source, converter, equalizer->eq, sink, nullptr);
    gst_element_link_many(source, converter, equalizer->eq, sink, nullptr);

    g_object_set(source, "volume", 1.0, nullptr);
    g_object_set(source, "mute", 0, nullptr);
    g_object_set(source, "provide-clock", 0, nullptr);
    g_object_set(source, "slave-method", 1, nullptr);
    g_object_set(source, "do-timestamp", 1, nullptr);

    g_object_set(sink, "volume", 1.0, nullptr);
    g_object_set(sink, "mute", 0, nullptr);
    g_object_set(sink, "provide-clock", 1, nullptr);

    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    std::string pulse_props = "application.id=com.github.pulse0ne.eqnix.sinkinputs";
    set_pulseaudio_props(pulse_props);
    set_source_monitor_name(pam->apps_sink_info->monitor_source_name);
    set_caps(pam->apps_sink_info->rate);

    auto PULSE_SINK = std::getenv("PULSE_SINK");
    if (PULSE_SINK != nullptr) {
        if (pam->get_sink_info(PULSE_SINK)) {
            set_output_sink_name(PULSE_SINK);
        } else {
            set_output_sink_name(pam->server_info.default_sink_name);
        }
    } else {
        bool use_default_sink = true; // g_settings_get_boolean(settings, "use-default-sink") != 0;

        if (use_default_sink) {
            set_output_sink_name(pam->server_info.default_sink_name);
        } else {
            //   gchar* custom_sink = g_settings_get_string(settings, "custom-sink");

            //   if (pm->get_sink_info(custom_sink)) {
            //     set_output_sink_name(custom_sink);
            //   } else {
            // set_output_sink_name(pm->server_info.default_sink_name);
            //   }

            //   g_free(custom_sink);
        }
    }

    pam->sink_input_added.connect(sigc::mem_fun(*this, &Pipeline::on_app_added));
    pam->sink_input_changed.connect(sigc::mem_fun(*this, &Pipeline::on_app_changed));
    pam->sink_input_removed.connect(sigc::mem_fun(*this, &Pipeline::on_app_removed));
    pam->sink_changed.connect(sigc::mem_fun(*this, &Pipeline::on_sink_changed));
}

Pipeline::~Pipeline() {
    set_null_pipeline();

    gst_object_unref(bus);
    gst_object_unref(pipeline);
}

GstElement* Pipeline::ensure_factory_create(std::string factory, std::string name) {
    GstElement* el = gst_element_factory_make(factory.c_str(), name.c_str());
    if (!el) {
        throw std::runtime_error("Failed to create element: " + factory);
    }
    return el;
}

void Pipeline::set_source_monitor_name(const std::string& name) {
    gchar* current_device;

    g_object_get(source, "current-device", &current_device, nullptr);

    if (name != current_device) {
        if (playing) {
            set_null_pipeline();

            g_object_set(source, "device", name.c_str(), nullptr);

            gst_element_set_state(pipeline, GST_STATE_PLAYING);
        } else {
            g_object_set(source, "device", name.c_str(), nullptr);
        }

        logger.debug("using input device: " + name);
    }

    g_free(current_device);
}

void Pipeline::set_output_sink_name(const std::string& name) {
    g_object_set(sink, "device", name.c_str(), nullptr);

    logger.debug("using output device: " + name);
}

void Pipeline::set_pulseaudio_props(const std::string& props) {
    auto str = "props," + props;

    auto s = gst_structure_from_string(str.c_str(), nullptr);

    g_object_set(source, "stream-properties", s, nullptr);
    g_object_set(sink, "stream-properties", s, nullptr);

    gst_structure_free(s);
}

void Pipeline::set_null_pipeline() {
    gst_element_set_state(pipeline, GST_STATE_NULL);

    GstState state;
    GstState pending;

    gst_element_get_state(pipeline, &state, &pending, state_check_timeout);

    /*on_message_state is not called when going to null. I don't know why.
     * so we have to update the variable manually after setting to null.
     */

    if (state == GST_STATE_NULL) {
        playing = false;
    }

    logger.debug(gst_element_state_get_name(state) + std::string(" -> ") + gst_element_state_get_name(pending));
}

auto Pipeline::apps_want_to_play() -> bool {
    bool wants_to_play = false;
    for (auto& a : apps_list) {
        if (a->wants_to_play) {
            wants_to_play = true;
            break;
        }
    }
    return wants_to_play;
}

void Pipeline::update_pipeline_state() {
    bool wants_to_play = apps_want_to_play();

    GstState state;
    GstState pending;

    gst_element_get_state(pipeline, &state, &pending, state_check_timeout);

    if (state != GST_STATE_PLAYING && wants_to_play) {
        // timeout_connection.disconnect();

        gst_element_set_state(pipeline, GST_STATE_PLAYING);
    } else if (state == GST_STATE_PLAYING && !wants_to_play) {
        // timeout_connection.disconnect();

        // timeout_connection = Glib::signal_timeout().connect_seconds(
        //     [=]() {
        //       GstState s;
        //       GstState p;

        //       gst_element_get_state(pipeline, &s, &p, state_check_timeout);

        //       if (s == GST_STATE_PLAYING && !apps_want_to_play()) {
        //         util::debug(log_tag + "No app wants to play audio. We will pause our pipeline.");

        //         gst_element_set_state(pipeline, GST_STATE_PAUSED);
        //       }

        //       return false;
        //     },
        //     5);
    }
}

// void Pipeline::get_latency() {
//   GstQuery* q = gst_query_new_latency();

//   if (gst_element_query(pipeline, q) != 0) {
//     gboolean live;
//     GstClockTime min;
//     GstClockTime max;

//     gst_query_parse_latency(q, &live, &min, &max);

//     int latency = GST_TIME_AS_MSECONDS(min);

//     util::debug(log_tag + "total latency: " + std::to_string(latency) + " ms");

//     Glib::signal_idle().connect_once([=] { new_latency.emit(latency); });
//   }

//   gst_query_unref(q);
// }

void Pipeline::set_caps(const uint& sampling_rate) {
    logger.debug(std::to_string(current_rate));
    current_rate = sampling_rate;
    logger.debug(std::to_string(current_rate));

    auto caps_str = "audio/x-raw,format=F32LE,channels=2,rate=" + std::to_string(sampling_rate);

    auto caps = gst_caps_from_string(caps_str.c_str());

    g_object_set(capsfilter, "caps", caps, nullptr);

    gst_caps_unref(caps);
}

void Pipeline::on_app_added(const std::shared_ptr<AppInfo>& app_info) {
    for (const auto& a : apps_list) {
        if (a->index == app_info->index) {
            return; // do not add the same app two times
        }
    }
    apps_list.push_back(app_info);
    update_pipeline_state();

    auto enable_all = true; // g_settings_get_boolean(settings, "enable-all-sinkinputs");

    if ((enable_all != 0) && !app_info->connected) {
        pam->move_sink_input_to_eqnix(app_info->name, app_info->index);
    }
}

void Pipeline::on_app_changed(const std::shared_ptr<AppInfo>& app_info) {
    std::replace_copy_if(
        apps_list.begin(), apps_list.end(), apps_list.begin(), [=](auto& a) { return a->index == app_info->index; }, app_info);
    update_pipeline_state();
}

void Pipeline::on_app_removed(uint idx) {
    apps_list.erase(std::remove_if(apps_list.begin(), apps_list.end(), [=](auto& a) { return a->index == idx; }), apps_list.end());
    update_pipeline_state();
}

void Pipeline::on_sink_changed(const std::shared_ptr<SinkInfo>& sink_info) {
    if (sink_info->name == "eqnix_apps") {
        if (sink_info->rate != current_rate) {
            gst_element_set_state(pipeline, GST_STATE_READY);
            set_caps(sink_info->rate);
            update_pipeline_state();
        }
    }
}

void Pipeline::on_source_changed(const std::shared_ptr<SourceInfo>& source_info) {
    if (source_info->name == "eqnix_mic.monitor") {
        if (source_info->rate != current_rate) {
            gst_element_set_state(pipeline, GST_STATE_READY);
            set_caps(source_info->rate);
            update_pipeline_state();
        }
    }
}