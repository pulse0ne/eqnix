#include <stdexcept>
#include "equalizer.hpp"

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
    ///  TODO: may be able to use the same internal gstiirequalizer.c math to calculate frequency response from transfer function

    bin = gst_bin_new("eq-bin");

    for (auto i = 0; i < NUM_NODES; i++) {
        std::string name = "node" + std::to_string(i);
        GstElement* node = gst_element_factory_make("biquad", name.c_str());
        if (!node) {
            throw std::runtime_error("Required gstreamer plugin (libbiquad.so) is not installed");
        }
        nodes.push_back(node);
        gst_bin_add(GST_BIN(bin), node);
    }
}

Equalizer::~Equalizer() {}
