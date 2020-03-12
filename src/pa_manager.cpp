#include "pa_manager.hpp"

PAManager::PAManager() : main_loop(pa_threaded_mainloop_new()), main_loop_api(pa_threaded_mainloop_get_api(main_loop)) {
    pa_threaded_mainloop_lock(main_loop);
    pa_threaded_mainloop_start(main_loop);

    context = pa_context_new(main_loop_api, "eqnix");

    pa_context_set_state_callback(context, &PAManager::context_state_cb, this);
    pa_context_connect(context, nullptr, PA_CONTEXT_NOFAIL, nullptr);
    pa_threaded_mainloop_wait(main_loop);
    pa_threaded_mainloop_unlock(main_loop);

    if (context_ready) {
        get_server_info();
        load_apps_sink();
        subscribe_to_events();
    } else {
        logger.error("context initialization failed");
    }
}

PAManager::~PAManager() {
    unload_sinks();
    drain_context();
    pa_threaded_mainloop_lock(main_loop);
    pa_context_disconnect(context);
    pa_context_unref(context);
    pa_threaded_mainloop_unlock(main_loop);
    pa_threaded_mainloop_stop(main_loop);
    pa_threaded_mainloop_free(main_loop);
}

void PAManager::context_state_cb(pa_context* ctx, void* data) {
    auto pm = static_cast<PAManager*>(data);
    auto state = pa_context_get_state(ctx);
    if (state == PA_CONTEXT_UNCONNECTED) {
        pm->logger.debug("context is unconnected");
    } else if (state == PA_CONTEXT_CONNECTING) {
        pm->logger.debug("context is connecting");
    } else if (state == PA_CONTEXT_AUTHORIZING) {
        pm->logger.debug("context is authorizing");
    } else if (state == PA_CONTEXT_SETTING_NAME) {
        pm->logger.debug("context is setting name");
    } else if (state == PA_CONTEXT_READY) {
        pm->logger.debug("context is ready");
        pm->logger.debug("connected to: " + std::string(pa_context_get_server(ctx)));
        auto protocol = std::to_string(pa_context_get_protocol_version(ctx));
        pm->server_info.protocol = protocol;
        pm->logger.debug("protocol version: " + protocol);
        pm->context_ready = true;
        pa_threaded_mainloop_signal(pm->main_loop, 0);
    } else if (state == PA_CONTEXT_FAILED) {
        pm->logger.debug("failed to connect context");
        pm->context_ready = false;
        pa_threaded_mainloop_signal(pm->main_loop, 0);
    } else if (state == PA_CONTEXT_TERMINATED) {
        pm->logger.debug("context was terminated");
        pm->context_ready = false;
        pa_threaded_mainloop_signal(pm->main_loop, 0);
    }
}

void PAManager::subscribe_to_events() {
    pa_context_set_subscribe_callback(context, [](auto c, auto t, auto idx, auto d) {
        auto f = t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK;

        auto pm = static_cast<PAManager*>(d);

        if (f == PA_SUBSCRIPTION_EVENT_SINK_INPUT) {
            auto e = t & PA_SUBSCRIPTION_EVENT_TYPE_MASK;

            if (e == PA_SUBSCRIPTION_EVENT_NEW) {
                pa_context_get_sink_input_info(c, idx, [](auto cx, auto info, auto eol, auto d) {
                    if (info != nullptr) {
                        auto pm = static_cast<PAManager*>(d);
                        pm->new_app(info);
                    }
                }, pm);
            } else if (e == PA_SUBSCRIPTION_EVENT_CHANGE) {
                pa_context_get_sink_input_info(c, idx, [](auto cx, auto info, auto eol, auto d) {
                    if (info != nullptr) {
                        auto pm = static_cast<PAManager*>(d);
                        pm->changed_app(info);
                    }
                }, pm);
            } else if (e == PA_SUBSCRIPTION_EVENT_REMOVE) {
                Glib::signal_idle().connect_once([pm, idx]() { pm->sink_input_removed.emit(idx); });
            }
        } else if (f == PA_SUBSCRIPTION_EVENT_SINK) {
            auto e = t & PA_SUBSCRIPTION_EVENT_TYPE_MASK;

            if (e == PA_SUBSCRIPTION_EVENT_NEW) {
                pa_context_get_sink_info_by_index(c, idx, [](auto cx, auto info, auto eol, auto d) {
                    if (info != nullptr) {
                        std::string s1 = "eqnix_apps";
                        std::string s2 = "eqnix_mic";

                        if (info->name != s1 && info->name != s2) {
                            auto pm = static_cast<PAManager*>(d);
                            auto si = std::make_shared<SinkInfo>();
                            si->name = info->name;
                            si->index = info->index;
                            si->description = info->description;
                            si->rate = info->sample_spec.rate;
                            si->format = pa_sample_format_to_string(info->sample_spec.format);
                            if (info->active_port != nullptr) {
                                si->active_port = info->active_port->name;
                            } else {
                                si->active_port = "null";
                            }
                            Glib::signal_idle().connect_once([pm, si = move(si)] { pm->sink_added.emit(si); });
                        }
                    }
                }, pm);
            } else if (e == PA_SUBSCRIPTION_EVENT_CHANGE) {
                pa_context_get_sink_info_by_index(c, idx, [](auto cx, auto info, auto eol, auto d) {
                    if (info != nullptr) {
                        auto pm = static_cast<PAManager*>(d);
                        auto si = std::make_shared<SinkInfo>();
                        si->name = info->name;
                        si->index = info->index;
                        si->description = info->description;
                        si->rate = info->sample_spec.rate;
                        si->format = pa_sample_format_to_string(info->sample_spec.format);
                        if (info->active_port != nullptr) {
                            si->active_port = info->active_port->name;
                        } else {
                            si->active_port = "null";
                        }
                        if (si->name == "eqnix_apps") {
                            pm->apps_sink_info->rate = si->rate;
                            pm->apps_sink_info->format = si->format;
                        }
                        Glib::signal_idle().connect_once([pm, si = move(si)] { pm->sink_changed.emit(si); });
                    }
                }, pm);
            } else if (e == PA_SUBSCRIPTION_EVENT_REMOVE) {
                Glib::signal_idle().connect_once([pm, idx]() { pm->sink_removed.emit(idx); });
            }
        } else if (f == PA_SUBSCRIPTION_EVENT_SERVER) {
            auto e = t & PA_SUBSCRIPTION_EVENT_TYPE_MASK;
            if (e == PA_SUBSCRIPTION_EVENT_CHANGE) {
                pa_context_get_server_info(c, [](auto cx, auto info, auto d) {
                    if (info != nullptr) {
                        auto pm = static_cast<PAManager*>(d);
                        pm->update_server_info(info);
                        std::string sink = info->default_sink_name;
                        std::string source = info->default_source_name;
                        if (sink != std::string("eqnix_apps")) {
                            Glib::signal_idle().connect_once([pm, sink]() { pm->new_default_sink.emit(sink); });
                        }
                        if (source != std::string("eqnix_mic.monitor")) {
                            Glib::signal_idle().connect_once([pm, source]() { pm->new_default_source.emit(source); });
                        }
                        Glib::signal_idle().connect_once([pm]() { pm->server_changed.emit(); });
                    }
                }, pm);
            }
        }
    }, this);

    auto mask = static_cast<pa_subscription_mask_t>(PA_SUBSCRIPTION_MASK_SINK_INPUT | PA_SUBSCRIPTION_MASK_SINK | PA_SUBSCRIPTION_MASK_SERVER);
    pa_context_subscribe(context, mask, [](auto c, auto success, auto d) {
        auto pm = static_cast<PAManager*>(d);

        if (success == 0) {
            pm->logger.critical("context event subscribe failed!");
        }
    }, this);
}

void PAManager::update_server_info(const pa_server_info* info) {
    server_info.server_name = info->server_name;
    server_info.server_version = info->server_version;
    server_info.default_sink_name = info->default_sink_name;
    server_info.default_source_name = info->default_source_name;
    server_info.format = pa_sample_format_to_string(info->sample_spec.format);
    server_info.rate = info->sample_spec.rate;
    server_info.channels = info->sample_spec.channels;

    if (pa_channel_map_to_pretty_name(&info->channel_map) != nullptr) {
        server_info.channel_map = pa_channel_map_to_pretty_name(&info->channel_map);
    } else {
        server_info.channel_map = "unknown";
    }
}

void PAManager::get_server_info() {
    pa_threaded_mainloop_lock(main_loop);

    auto o = pa_context_get_server_info(context, [](auto c, auto info, auto d) {
        auto pm = static_cast<PAManager*>(d);
        if (info != nullptr) {
            pm->update_server_info(info);
            pm->logger.debug("Pulseaudio version: " + std::string(info->server_version));
            pm->logger.debug("default pulseaudio source: " + std::string(info->default_source_name));
            pm->logger.debug("default pulseaudio sink: " + std::string(info->default_sink_name));
        }
        pa_threaded_mainloop_signal(pm->main_loop, false);
    }, this);

    if (o != nullptr) {
        while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) {
            pa_threaded_mainloop_wait(main_loop);
        }
        pa_operation_unref(o);
    } else {
        logger.critical("failed to get server info");
    }
    pa_threaded_mainloop_unlock(main_loop);
}

auto PAManager::get_sink_info(const std::string& name) -> std::shared_ptr<SinkInfo> {
    auto si = std::make_shared<SinkInfo>();
    struct Data {
        bool failed;
        PAManager* pm;
        std::shared_ptr<SinkInfo> si;
    };
    Data data = {false, this, si};
    pa_threaded_mainloop_lock(main_loop);
    auto o = pa_context_get_sink_info_by_name(context, name.c_str(), [](auto c, auto info, auto eol, auto data) {
        auto d = static_cast<Data*>(data);

        if (eol < 0) {
            d->failed = true;
            pa_threaded_mainloop_signal(d->pm->main_loop, false);
        } else if (eol > 0) {
            pa_threaded_mainloop_signal(d->pm->main_loop, false);
        } else if (info != nullptr) {
            d->si->name = info->name;
            d->si->index = info->index;
            d->si->description = info->description;
            d->si->owner_module = info->owner_module;
            d->si->monitor_source = info->monitor_source;
            d->si->monitor_source_name = info->monitor_source_name;
            d->si->rate = info->sample_spec.rate;
            d->si->format = pa_sample_format_to_string(info->sample_spec.format);

            if (info->active_port != nullptr) {
                d->si->active_port = info->active_port->name;
            } else {
                d->si->active_port = "null";
            }
        }
    }, &data);

    if (o != nullptr) {
        while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) {
            pa_threaded_mainloop_wait(main_loop);
        }
        pa_operation_unref(o);
    } else {
        logger.critical(" failed to get sink info: " + name);
    }
    pa_threaded_mainloop_unlock(main_loop);
    if (!data.failed) {
        return si;
    }
    logger.debug("failed to get sink info: " + name);
    return nullptr;
}

auto PAManager::get_source_info(const std::string& name) -> std::shared_ptr<SourceInfo> {
    auto si = std::make_shared<SourceInfo>();
    struct Data {
        bool failed;
        PAManager* pm;
        std::shared_ptr<SourceInfo> si;
    };
    Data data = {false, this, si};
    pa_threaded_mainloop_lock(main_loop);
    auto o = pa_context_get_source_info_by_name(context, name.c_str(), [](auto c, auto info, auto eol, auto data) {
        auto d = static_cast<Data*>(data);
        if (eol < 0) {
            d->failed = true;
            pa_threaded_mainloop_signal(d->pm->main_loop, false);
        } else if (eol > 0) {
            pa_threaded_mainloop_signal(d->pm->main_loop, false);
        } else if (info != nullptr) {
            d->si->name = info->name;
            d->si->index = info->index;
            d->si->description = info->description;
            d->si->rate = info->sample_spec.rate;
            d->si->format = pa_sample_format_to_string(info->sample_spec.format);

            if (info->active_port != nullptr) {
                d->si->active_port = info->active_port->name;
            } else {
                d->si->active_port = "null";
            }
        }
    }, &data);

    if (o != nullptr) {
        while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) {
            pa_threaded_mainloop_wait(main_loop);
        }
        pa_operation_unref(o);
    } else {
        logger.critical("failed to get source info:" + name);
    }
    pa_threaded_mainloop_unlock(main_loop);
    if (!data.failed) {
        return si;
    }
    logger.debug("failed to get source info:" + name);
    return nullptr;
}

auto PAManager::get_default_sink_info() -> std::shared_ptr<SinkInfo> {
    auto info = get_sink_info(server_info.default_sink_name);
    if (info != nullptr) {
        logger.debug("default pulseaudio sink sampling rate: " + std::to_string(info->rate) + " Hz");
        logger.debug("default pulseaudio sink audio format: " + info->format);

        return info;
    }
    logger.critical("could not get default sink info");
    return nullptr;
}

auto PAManager::get_default_source_info() -> std::shared_ptr<SourceInfo> {
    auto info = get_source_info(server_info.default_source_name);
    if (info != nullptr) {
        logger.debug("default pulseaudio source sampling rate: " + std::to_string(info->rate) + " Hz");
        logger.debug("default pulseaudio source audio format: " + info->format);

        return info;
    }
    logger.critical("could not get default source info");
    return nullptr;
}

auto PAManager::load_module(const std::string& name, const std::string& argument) -> bool {
    struct Data {
        bool status;
        PAManager* pm;
    };
    Data data = {false, this};
    pa_threaded_mainloop_lock(main_loop);
    auto o = pa_context_load_module(context, name.c_str(), argument.c_str(), [](auto c, auto idx, auto data) {
        auto d = static_cast<Data*>(data);
        d->status = idx != PA_INVALID_INDEX;
        pa_threaded_mainloop_signal(d->pm->main_loop, false);
    }, &data);

    if (o != nullptr) {
        while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) {
            pa_threaded_mainloop_wait(main_loop);
        }
        pa_operation_unref(o);
    }
    pa_threaded_mainloop_unlock(main_loop);
    return data.status;
}

auto PAManager::load_sink(const std::string& name, const std::string& description, uint rate) -> std::shared_ptr<SinkInfo> {
    auto si = get_sink_info(name);
    if (si == nullptr) {  // sink is not loaded
        std::string argument = "sink_name=" + name + " " + "sink_properties=" + description + "device.class=\"sound\"" + " " + "norewinds=1";
        bool ok = load_module("module-null-sink", argument);
        if (ok) {
            logger.debug("loaded module-null-sink: " + argument);
            si = get_sink_info(name);
        } else {
            logger.warn("Pulseaudio " + server_info.server_version + " does not support norewinds. Loading the sink the old way. Changing apps volume will cause cracklings");
            argument = "sink_name=" + name + " " + "sink_properties=" + description + "device.class=\"sound\"" + " " + "channels=2" + " " + "rate=" + std::to_string(rate);
            ok = load_module("module-null-sink", argument);
            if (ok) {
                logger.debug("loaded module-null-sink: " + argument);
                si = get_sink_info(name);
            } else {
                logger.critical("failed to load module-null-sink with argument: " + argument);
            }
        }
    }
    return si;
}

void PAManager::load_apps_sink() {
    logger.debug("loading eqnix applications output sink...");
    auto info = get_default_sink_info();
    if (info != nullptr) {
        std::string name = "eqnix_apps";
        std::string description = "device.description=\"eqnix(apps)\"";
        auto rate = info->rate;
        apps_sink_info = load_sink(name, description, rate);
    }
}

void PAManager::find_sink_inputs() {
    pa_threaded_mainloop_lock(main_loop);
    auto o = pa_context_get_sink_input_info_list(context, [](auto c, auto info, auto eol, auto d) {
        auto pm = static_cast<PAManager*>(d);

        if (info != nullptr) {
            pm->new_app(info);
        } else {
            pa_threaded_mainloop_signal(pm->main_loop, false);
        }
    }, this);

    if (o != nullptr) {
        while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) {
            pa_threaded_mainloop_wait(main_loop);
        }
        pa_operation_unref(o);
    } else {
        logger.warn("failed to find sink inputs");
    }
    pa_threaded_mainloop_unlock(main_loop);
}

void PAManager::find_sinks() {
    pa_threaded_mainloop_lock(main_loop);
    auto o = pa_context_get_sink_info_list(context, [](auto c, auto info, auto eol, auto d) {
        auto pm = static_cast<PAManager*>(d);
        if (info != nullptr) {
            std::string s1 = "eqnix_apps";
            std::string s2 = "eqnix_mic";
            if (info->name != s1 && info->name != s2) {
                auto si = std::make_shared<SinkInfo>();
                si->name = info->name;
                si->index = info->index;
                si->description = info->description;
                si->rate = info->sample_spec.rate;
                si->format = pa_sample_format_to_string(info->sample_spec.format);

                Glib::signal_idle().connect_once([pm, si = move(si)] { pm->sink_added.emit(si); });
            }
        } else {
            pa_threaded_mainloop_signal(pm->main_loop, false);
        }
    }, this);

    if (o != nullptr) {
        while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) {
            pa_threaded_mainloop_wait(main_loop);
        }
        pa_operation_unref(o);
    } else {
        logger.warn("failed to find sinks");
    }
    pa_threaded_mainloop_unlock(main_loop);
}

void PAManager::move_sink_input_to_eqnix(const std::string& name, uint idx) {
    struct Data {
        std::string name;
        uint idx;
        PAManager* pm;
    };
    Data data = {name, idx, this};
    pa_threaded_mainloop_lock(main_loop);
    auto o = pa_context_move_sink_input_by_index(context, idx, apps_sink_info->index, [](auto c, auto success, auto data) {
        auto d = static_cast<Data*>(data);
        if (success) {
            d->pm->logger.debug("sink input: " + d->name + ", idx = " + std::to_string(d->idx) + " moved to PE");
        } else {
            d->pm->logger.critical("failed to move sink input: " + d->name + ", idx = " + std::to_string(d->idx) + " to PE");
        }
        pa_threaded_mainloop_signal(d->pm->main_loop, false);
    }, &data);

    if (o != nullptr) {
        while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) {
            pa_threaded_mainloop_wait(main_loop);
        }
        pa_operation_unref(o);
    } else {
        logger.critical("failed to move sink input: " + name + ", idx = " + std::to_string(idx) + " to PE");
    }
    pa_threaded_mainloop_unlock(main_loop);
}

void PAManager::remove_sink_input_from_eqnix(const std::string& name, uint idx) {
    struct Data {
        std::string name;
        uint idx;
        PAManager* pm;
    };
    Data data = {name, idx, this};
    pa_threaded_mainloop_lock(main_loop);
    auto o = pa_context_move_sink_input_by_name(context, idx, server_info.default_sink_name.c_str(), [](auto c, auto success, auto data) {
        auto d = static_cast<Data*>(data);

        if (success) {
            d->pm->logger.debug("sink input: " + d->name + ", idx = " + std::to_string(d->idx) + " removed from PE");
        } else {
            d->pm->logger.critical("failed to remove sink input: " + d->name + ", idx = " + std::to_string(d->idx) + " from PE");
        }
        pa_threaded_mainloop_signal(d->pm->main_loop, false);
    }, &data);

    if (o != nullptr) {
        while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) {
            pa_threaded_mainloop_wait(main_loop);
        }
        pa_operation_unref(o);
    } else {
        logger.critical("failed to remove sink input: " + name + ", idx = " + std::to_string(idx) + " from PE");
    }
    pa_threaded_mainloop_unlock(main_loop);
}


void PAManager::set_sink_input_volume(const std::string& name, uint idx, uint8_t channels, uint value) {
    pa_volume_t raw_value = PA_VOLUME_NORM * value / 100.0;
    auto cvol = pa_cvolume();
    auto cvol_ptr = pa_cvolume_set(&cvol, channels, raw_value);
    if (cvol_ptr != nullptr) {
        struct Data {
            std::string name;
            uint idx;
            PAManager* pm;
        };
        Data data = {name, idx, this};
        pa_threaded_mainloop_lock(main_loop);
        auto o = pa_context_set_sink_input_volume(context, idx, cvol_ptr, [](auto c, auto success, auto data) {
            auto d = static_cast<Data*>(data);
            if (success == 1) {
                d->pm->logger.debug("changed volume of sink input: " + d->name + ", idx = " + std::to_string(d->idx));
            } else {
                d->pm->logger.critical("failed to change volume of sink input: " + d->name + ", idx = " + std::to_string(d->idx));
            }
            pa_threaded_mainloop_signal(d->pm->main_loop, false);
        }, &data);

        if (o != nullptr) {
            while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) {
                pa_threaded_mainloop_wait(main_loop);
            }
            pa_operation_unref(o);
            pa_threaded_mainloop_unlock(main_loop);
        } else {
            logger.warn("failed to change volume of sink input: " + name + ", idx = " + std::to_string(idx));
            pa_threaded_mainloop_unlock(main_loop);
        }
    }
}

void PAManager::set_sink_input_mute(const std::string& name, uint idx, bool state) {
    struct Data {
        std::string name;
        uint idx;
        PAManager* pm;
    };
    Data data = {name, idx, this};
    pa_threaded_mainloop_lock(main_loop);
    auto o = pa_context_set_sink_input_mute(context, idx, static_cast<int>(state), [](auto c, auto success, auto data) {
        auto d = static_cast<Data*>(data);
        if (success == 1) {
            d->pm->logger.debug("sink input: " + d->name + ", idx = " + std::to_string(d->idx) + " is muted");
        } else {
            d->pm->logger.critical("failed to mute sink input: " + d->name + ", idx = " + std::to_string(d->idx));
        }
        pa_threaded_mainloop_signal(d->pm->main_loop, false);
    }, &data);

    if (o != nullptr) {
        while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) {
            pa_threaded_mainloop_wait(main_loop);
        }
        pa_operation_unref(o);
    } else {
        logger.warn("failed to mute set sink input: " + name + ", idx = " + std::to_string(idx) + " to PE");
    }
    pa_threaded_mainloop_unlock(main_loop);
}

void PAManager::get_sink_input_info(uint idx) {
    pa_threaded_mainloop_lock(main_loop);
    auto o = pa_context_get_sink_input_info(context, idx, [](auto c, auto info, auto eol, auto d) {
        auto pm = static_cast<PAManager*>(d);
        if (info != nullptr) {
            pm->changed_app(info);
        } else {
            pa_threaded_mainloop_signal(pm->main_loop, false);
        }
    }, this);

    if (o != nullptr) {
        while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) {
            pa_threaded_mainloop_wait(main_loop);
        }
        pa_operation_unref(o);
    } else {
        logger.critical("failed to get sink input info: " + std::to_string(idx));
    }
    pa_threaded_mainloop_unlock(main_loop);
}

void PAManager::get_modules_info() {
    pa_threaded_mainloop_lock(main_loop);
    auto o = pa_context_get_module_info_list(context, [](auto c, auto info, auto eol, auto d) {
        auto pm = static_cast<PAManager*>(d);
        if (info != nullptr) {
            auto mi = std::make_shared<ModuleInfo>();
            if (info->name) {
                mi->name = info->name;
                mi->index = info->index;
                if (info->argument) {
                    mi->argument = info->argument;
                } else {
                    mi->argument = "";
                }
                Glib::signal_idle().connect_once([pm, mi = move(mi)] { pm->module_info.emit(mi); });
            }
        } else {
            pa_threaded_mainloop_signal(pm->main_loop, false);
        }
    }, this);

    if (o != nullptr) {
        while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) {
            pa_threaded_mainloop_wait(main_loop);
        }
        pa_operation_unref(o);
    } else {
        logger.critical("failed to get modules info");
    }

    pa_threaded_mainloop_unlock(main_loop);
}

void PAManager::get_clients_info() {
    pa_threaded_mainloop_lock(main_loop);
    auto o = pa_context_get_client_info_list(context, [](auto c, auto info, auto eol, auto d) {
        auto pm = static_cast<PAManager*>(d);
        if (info != nullptr) {
            auto mi = std::make_shared<ClientInfo>();
            if (info->name) {
                mi->name = info->name;
                mi->index = info->index;
                if (pa_proplist_contains(info->proplist, "application.process.binary") == 1) {
                    mi->binary = pa_proplist_gets(info->proplist, "application.process.binary");
                } else {
                    mi->binary = "";
                }
                Glib::signal_idle().connect_once([pm, mi = move(mi)] { pm->client_info.emit(mi); });
            }
        } else {
            pa_threaded_mainloop_signal(pm->main_loop, false);
        }
    }, this);

    if (o != nullptr) {
        while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) {
            pa_threaded_mainloop_wait(main_loop);
        }
        pa_operation_unref(o);
    } else {
        logger.critical("failed to get clients info");
    }

    pa_threaded_mainloop_unlock(main_loop);
}

void PAManager::unload_module(uint idx) {
    struct Data {
        uint idx;
        PAManager* pm;
    };

    Data data = {idx, this};
    pa_threaded_mainloop_lock(main_loop);
    auto o = pa_context_unload_module(context, idx, [](auto c, auto success, auto data) {
        auto d = static_cast<Data*>(data);
        if (success) {
            d->pm->logger.debug("module " + std::to_string(d->idx) + " unloaded");
        } else {
            d->pm->logger.critical("failed to unload module " + std::to_string(d->idx));
        }
        pa_threaded_mainloop_signal(d->pm->main_loop, false);
    }, &data);

    if (o != nullptr) {
        while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) {
            pa_threaded_mainloop_wait(main_loop);
        }
        pa_operation_unref(o);
    } else {
        logger.critical("failed to unload module: " + std::to_string(idx));
    }
    pa_threaded_mainloop_unlock(main_loop);
}

void PAManager::unload_sinks() {
    logger.debug("unloading eqnix sinks...");
    unload_module(apps_sink_info->owner_module);
    // unload_module(mic_sink_info->owner_module);
}

void PAManager::drain_context() {
    pa_threaded_mainloop_lock(main_loop);

    auto o = pa_context_drain(context, [](auto c, auto d) {
        auto pm = static_cast<PAManager*>(d);
        if (pa_context_get_state(c) == PA_CONTEXT_READY) {
            pa_threaded_mainloop_signal(pm->main_loop, false);
        }
    }, this);

    if (o != nullptr) {
        while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) {
            pa_threaded_mainloop_wait(main_loop);
        }
        pa_operation_unref(o);
        pa_threaded_mainloop_unlock(main_loop);
        logger.debug("Context was drained");
    } else {
        pa_threaded_mainloop_unlock(main_loop);
        logger.debug("Context did not need draining");
    }
}

void PAManager::new_app(const pa_sink_input_info* info) {
    auto app_info = parse_app_info(info);
    if (app_info != nullptr) {
        // checking if the user blacklisted this app
        auto forbidden_app = std::find(std::begin(blacklist_out), std::end(blacklist_out), app_info->name) != std::end(blacklist_out);
        if (!forbidden_app) {
            app_info->app_type = "sink_input";
            Glib::signal_idle().connect_once([&, app_info = move(app_info)]() { sink_input_added.emit(app_info); });
        }
    }
}

void PAManager::changed_app(const pa_sink_input_info* info) {
    auto app_info = parse_app_info(info);
    if (app_info != nullptr) {
        // checking if the user blacklisted this app
        auto forbidden_app = std::find(std::begin(blacklist_out), std::end(blacklist_out), app_info->name) != std::end(blacklist_out);
        if (!forbidden_app) {
            app_info->app_type = "sink_input";
            Glib::signal_idle().connect_once([&, app_info = move(app_info)]() { sink_input_changed.emit(app_info); });
        }
    }
}

auto PAManager::app_is_connected(const pa_sink_input_info* info) -> bool {
    return info->sink == apps_sink_info->index;
}
