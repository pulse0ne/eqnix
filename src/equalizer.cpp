#include <stdexcept>
#include "equalizer.hpp"

Equalizer::Equalizer() {
    eq = gst_element_factory_make("equalizer-nbands", "eq");

    if (!eq) {
        throw std::runtime_error("Required gstreamer plugin (equalizer-nbands) is not installed");
    }
    fr_eq = gst_element_factory_make("equalizer-nbands", "fr-eq");

    g_object_set(eq, "num-bands", 1, nullptr);
    g_object_set(fr_eq, "num-bands", 1, nullptr);

    ////
    bands[0].enabled = true;
    bands[0].freq = 45.0;
    bands[0].gain = 12.0;
    bands[0].q = 20.0;
    // bands[1].enabled = true;
    // bands[1].freq = 800.0;
    // bands[1].gain = -20.0;
    // bands[1].q = 100.0;
    // bands[2].enabled = true;
    // bands[2].freq = 10000.0;
    // bands[2].gain = -20.0;
    // bands[2].q = 100.0;
    for (auto i = 0; i < 1; i++) {
        auto band = gst_child_proxy_get_child_by_index(GST_CHILD_PROXY(eq), i);
        auto frband = gst_child_proxy_get_child_by_index(GST_CHILD_PROXY(fr_eq), i);
        g_object_set(band, "freq", bands[i].freq, "bandwidth", bands[i].q, "gain", bands[i].gain, nullptr);
        g_object_set(frband, "freq", bands[i].freq, "bandwidth", bands[i].q, "gain", bands[i].gain, nullptr);
        g_object_unref(band);
        g_object_unref(frband);
    }
    ///
}

Equalizer::~Equalizer() {}
