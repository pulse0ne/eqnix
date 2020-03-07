#include <stdexcept>
#include "equalizer.hpp"

#include <cstring>


Equalizer::Equalizer() {
    bin = gst_element_factory_make("iirequalizer", "eq");
    if (!bin) {
        throw std::runtime_error("nope");
    }

    g_object_set(bin, "num-bands", 6, nullptr);

    GObject* band = gst_child_proxy_get_child_by_index(GST_CHILD_PROXY(bin), 0);
    g_object_set(band, "freq", 20.0, "gain", +8.0, "bandwidth", 50.0, "type", 0, nullptr);
    band = gst_child_proxy_get_child_by_index(GST_CHILD_PROXY(bin), 1);
    g_object_set(band, "freq", 40.0, "gain", +6.0, "bandwidth", 100.0, "type", 0, nullptr);
    band = gst_child_proxy_get_child_by_index(GST_CHILD_PROXY(bin), 2);
    g_object_set(band, "freq", 100.0, "gain", -7.0, "bandwidth", 100.0, "type", 0, nullptr);
    band = gst_child_proxy_get_child_by_index(GST_CHILD_PROXY(bin), 3);
    g_object_set(band, "freq", 450.0, "gain", -6.0, "bandwidth", 250.0, "type", 0, nullptr);
    band = gst_child_proxy_get_child_by_index(GST_CHILD_PROXY(bin), 4);
    g_object_set(band, "freq", 1750.0, "gain", +1.0, "bandwidth", 500.0, "type", 0, nullptr);
    band = gst_child_proxy_get_child_by_index(GST_CHILD_PROXY(bin), 5);
    g_object_set(band, "freq", 10000.0, "gain", +5.0, "bandwidth", 5000.0, "type", 0, nullptr);

    auto s = gst_structure_new_empty("filter-query");
    auto ev = gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, s); // takes ownership of s
    gst_element_send_event(bin, ev); // takes ownership of ev
}

Equalizer::~Equalizer() {
    // TODO cleanup
}
