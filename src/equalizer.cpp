#include <stdexcept>
#include "equalizer.hpp"

#include <thread>
#include <chrono>

#define NUM_NODES 1


Equalizer::Equalizer() {
///////////////////////
    // bin = gst_bin_new("eq-bin");

    // id_in = gst_element_factory_make("identity", nullptr);
    // id_out = gst_element_factory_make("identity", nullptr);

    // gst_bin_add_many(GST_BIN(bin), id_in, id_out, nullptr);

    // auto sinkpad = gst_element_get_static_pad(id_in, "sink");
    // auto srcpad = gst_element_get_static_pad(id_out, "src");

    // gst_element_add_pad(bin, gst_ghost_pad_new("sink", sinkpad));
    // gst_element_add_pad(bin, gst_ghost_pad_new("src", srcpad));

    // g_object_unref(sinkpad);
    // g_object_unref(srcpad);

    // for (auto i = 0; i < NUM_NODES; i++) {
    //     std::string name = "node" + std::to_string(i);
    //     GstElement* node = gst_element_factory_make("biquad", name.c_str());
    //     if (!node) {
    //         throw std::runtime_error("Required gstreamer plugin (libbiquad.so) is not installed");
    //     }
    //     g_object_set(node, "filtertype", 7, nullptr);
    //     nodes.push_back(node);
    //     gst_bin_add(GST_BIN(bin), node);
    // }

    // auto last = nodes[0];
    // gst_element_link(id_in, last);
    // for (auto i = 1; i < NUM_NODES; i++) {
    //     gst_element_link(last, nodes[i]);
    //     last = nodes[i];
    // }
    // gst_element_link(last, id_out);

    // /////
    // g_object_set(nodes[0], "frequency", 50.0, "gain", -8.0, "q", 1.0, "emitfr", TRUE, "frbands", 5, nullptr);
    // /////

    // std::thread([](GstElement* n) {
    //     g_print("sleeping\n");
    //     std::this_thread::sleep_for(std::chrono::seconds(5));
    //     g_print("here\n");
    //     g_object_set(n, "gain", 10.0, nullptr);
    // }, nodes[0]).detach();

    ///////////////////
    bin = gst_element_factory_make("iirequalizer", "eq");
    // bin = gst_element_factory_make("eqnixparaeq", nullptr);
    if (!bin) {
        throw std::runtime_error("nope");
    }

    g_object_set(bin, "num-bands", 6, nullptr);

    GObject* band = gst_child_proxy_get_child_by_index(GST_CHILD_PROXY(bin), 0);
    g_object_set(band, "freq", 20.0, "gain", +12.0, "bandwidth", 10.0, "type", 0, nullptr);
    band = gst_child_proxy_get_child_by_index(GST_CHILD_PROXY(bin), 1);
    g_object_set(band, "freq", 40.0, "gain", +9.0, "bandwidth", 100.0, "type", 0, nullptr);
    band = gst_child_proxy_get_child_by_index(GST_CHILD_PROXY(bin), 2);
    g_object_set(band, "freq", 100.0, "gain", -7.0, "bandwidth", 100.0, "type", 0, nullptr);
    band = gst_child_proxy_get_child_by_index(GST_CHILD_PROXY(bin), 3);
    g_object_set(band, "freq", 450.0, "gain", -6.0, "bandwidth", 100.0, "type", 0, nullptr);
    band = gst_child_proxy_get_child_by_index(GST_CHILD_PROXY(bin), 4);
    g_object_set(band, "freq", 1750.0, "gain", +1.0, "bandwidth", 100.0, "type", 0, nullptr);
    band = gst_child_proxy_get_child_by_index(GST_CHILD_PROXY(bin), 5);
    g_object_set(band, "freq", 10000.0, "gain", +5.0, "bandwidth", 100.0, "type", 0, nullptr);
}

Equalizer::~Equalizer() {
    // TODO cleanup
}
