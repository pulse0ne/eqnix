#include <stdexcept>
#include "equalizer.hpp"

Equalizer::Equalizer() {
    eq = gst_element_factory_make("equalizer-nbands", "eq");

    if (!eq) {
        throw std::runtime_error("Required gstreamer plugin (equalizer-nbands) is not installed");
    }

    g_object_set(eq, "num-bands", 2, nullptr);

    ////
    bands[0].enabled = true;
    bands[0].set_band_type(BandType::PEAKING);
    bands[0].set_freq(45.0);
    bands[0].set_gain(12.0);
    bands[0].set_bw(20.0);
    bands[1].enabled = true;
    bands[1].set_band_type(BandType::PEAKING);
    bands[1].set_freq(800.0);
    bands[1].set_gain(-12.0); //-20.0;
    bands[1].set_bw(100.0);
    for (auto i = 0; i < 2; i++) {
        auto band = gst_child_proxy_get_child_by_index(GST_CHILD_PROXY(eq), i);
        g_object_set(band, "freq", bands[i].get_freq(), "bandwidth", bands[i].get_bw(), "gain", bands[i].get_gain(), nullptr);
        g_object_unref(band);
    }

    std::vector<double> freqs = {40.0, 100.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0};

    std::vector<double> mr(freqs.size());
    for (auto i = 0; i < 2; i++) {
        mr = bands[i].get_frequency_response(freqs);
        g_print("[");
        for (auto a : mr) {
            g_print(" %f ", a);
        }
        g_print("]\n");
    }
    ///  TODO: may be able to use the same internal gstiirequalizer.c math to calculate frequency response from transfer function
}

Equalizer::~Equalizer() {}
