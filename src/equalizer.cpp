#include <stdexcept>
#include "equalizer.hpp"

#include <thread>
#include <chrono>

#define NUM_NODES 8

Equalizer::Equalizer() {
    // eq = gst_element_factory_make("equalizer-nbands", "eq");

    // if (!eq) {
    //     throw std::runtime_error("Required gstreamer plugin (equalizer-nbands) is not installed");
    // }

    // g_object_set(eq, "num-bands", 2, nullptr);

    // ////
    // bands[0].enabled = true;
    // bands[0].set_band_type(BandType::PEAKING);
    // bands[0].set_freq(45.0);
    // bands[0].set_gain(12.0);
    // bands[0].set_bw(20.0);
    // bands[1].enabled = true;
    // bands[1].set_band_type(BandType::PEAKING);
    // bands[1].set_freq(800.0);
    // bands[1].set_gain(-12.0); //-20.0;
    // bands[1].set_bw(100.0);
    // for (auto i = 0; i < 2; i++) {
    //     auto band = gst_child_proxy_get_child_by_index(GST_CHILD_PROXY(eq), i);
    //     g_object_set(band, "freq", bands[i].get_freq(), "bandwidth", bands[i].get_bw(), "gain", bands[i].get_gain(), nullptr);
    //     g_object_unref(band);
    // }

    bin = gst_bin_new("eq-bin");

    id_in = gst_element_factory_make("identity", nullptr);
    id_out = gst_element_factory_make("identity", nullptr);

    gst_bin_add_many(GST_BIN(bin), id_in, id_out, nullptr);

    auto sinkpad = gst_element_get_static_pad(id_in, "sink");
    auto srcpad = gst_element_get_static_pad(id_out, "src");

    gst_element_add_pad(bin, gst_ghost_pad_new("sink", sinkpad));
    gst_element_add_pad(bin, gst_ghost_pad_new("src", srcpad));

    g_object_unref(sinkpad);
    g_object_unref(srcpad);

    for (auto i = 0; i < NUM_NODES; i++) {
        std::string name = "node" + std::to_string(i);
        GstElement* node = gst_element_factory_make("biquad", name.c_str());
        if (!node) {
            throw std::runtime_error("Required gstreamer plugin (libbiquad.so) is not installed");
        }
        nodes.push_back(node);
        gst_bin_add(GST_BIN(bin), node);
    }
    
    auto last = nodes[0];
    gst_element_link(id_in, last);
    for (auto i = 1; i < NUM_NODES; i++) {
        gst_element_link(last, nodes[i]);
        last = nodes[i];
    }
    gst_element_link(last, id_out);

    /////
    g_object_set(nodes[0], "frequency", 40.0, "gain", 30.0, "q", 1.0, "emitfr", TRUE, "frbands", 10, nullptr);
    /////

    std::thread([](GstElement* n) {
        g_print("sleeping\n");
        std::this_thread::sleep_for(std::chrono::seconds(7));
        g_print("here\n");
        g_object_set(n, "gain", 15.0, nullptr);
    }, nodes[0]).detach();
}

Equalizer::~Equalizer() {
    // TODO cleanup
}
