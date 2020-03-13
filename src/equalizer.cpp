#include <stdexcept>
#include "equalizer.hpp"

#include <cstring>

#define NUM_BANDS 8

Equalizer::Equalizer() {
    bin = gst_element_factory_make("iirequalizer", "eq");
    if (!bin) {
        throw std::runtime_error("nope"); // TODO
    }

    g_object_set(bin, "num-bands", NUM_BANDS, nullptr);

    // TODO: replace below with saved settings
    for (auto i = 0; i < NUM_BANDS; ++i) {
        GObject* band = gst_child_proxy_get_child_by_index(GST_CHILD_PROXY(bin), i);
        if (band) {
            g_object_set(band, "gain", 0.0, "q", 1.0, nullptr);
        }
    }

    change_filter.connect(sigc::mem_fun(*this, &Equalizer::handle_change_filter));
}

Equalizer::~Equalizer() {
    // TODO cleanup
}

void Equalizer::handle_change_filter(std::string name, FilterChangeType change_type, const GValue* value) {
    GObject* band = gst_child_proxy_get_child_by_name(GST_CHILD_PROXY(bin), name.c_str());
    if (band) {
        if (change_type == FilterChangeType::TYPE) {
            uint type = g_value_get_uint(value);
            g_object_set(band, "type", type, nullptr);
        } else {
            std::string prop;
            double val = g_value_get_double(value);
            switch(change_type) {
            case Q: prop = "q"; break;
            case FREQ: prop = "freq"; break;
            case GAIN: prop = "gain"; break;
            default:
                return;
            }
            g_object_set(band, prop.c_str(), val, nullptr);
        }
    }
}
