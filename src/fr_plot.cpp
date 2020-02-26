#include "fr_plot.hpp"
#include <cmath>
#include <iomanip>

namespace {
    gboolean on_spectrum_message(GstBus* bus, GstMessage* msg, gpointer data) {
        if (msg->type == GstMessageType::GST_MESSAGE_ELEMENT) {
            FrequencyResponsePlot* fr = static_cast<FrequencyResponsePlot*>(data);
            const GstStructure* s = gst_message_get_structure(msg);
            const std::string name = gst_structure_get_name(s);

            if (name == "spectrum") {
                const GValue *magnitudes, *mag;
                magnitudes = gst_structure_get_value(s, "magnitude");
                g_print("[");
                for (auto i = 0; i < 20; i++) {
                    mag = gst_value_list_get_value(magnitudes, i);
                    float fmag = g_value_get_float(mag);
                    g_print(" %f ", fmag);
                }
                g_print("]\n");
            }
        }
        return true;
    }
}

FrequencyResponsePlot::FrequencyResponsePlot(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& builder, Colormap _colors, std::shared_ptr<Equalizer> _equalizer)
    : Gtk::DrawingArea(cobject), nyquist(22050.0), start_freq(10.0), equalizer(_equalizer), colors(_colors) {
    add_events(Gdk::BUTTON_PRESS_MASK);

    pipeline = gst_pipeline_new("fr-pipeline");
    // src = gst_element_factory_make("fakesrc", "fr-src");
    src = gst_element_factory_make("audiotestsrc", "fr-src");
    spectrum = gst_element_factory_make("spectrum", "fr-spectrum");
    sink = gst_element_factory_make("fakesink", "fr-sink");

    bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    gst_bus_add_watch(bus, on_spectrum_message, this);

    gst_bin_add_many(GST_BIN(pipeline), src, equalizer->fr_eq, spectrum, sink, nullptr);
    gst_element_link_many(src, equalizer->fr_eq, spectrum, sink, nullptr);

    g_object_set(src, "wave", 5, "volume", 0.1, nullptr);
    // g_object_set(src, "filltype", 4, "pattern", "1", "sizetype", 1, "sync", 1, nullptr);
    g_object_set(spectrum, "interval", 1000000000, "bands", 20, "post-messages", 1, "threshold", -120, nullptr);
    g_object_set(sink, "sync", 1, nullptr);

    gst_element_set_state(pipeline, GstState::GST_STATE_PLAYING);
}

FrequencyResponsePlot::~FrequencyResponsePlot() {
    gst_element_set_state(pipeline, GstState::GST_STATE_NULL);

    gst_object_unref(bus);
    gst_object_unref(pipeline);
}

bool FrequencyResponsePlot::on_draw(const Cairo::RefPtr<Cairo::Context>& cr) {
    Gtk::Allocation allocation = get_allocation();
    const int w = allocation.get_width();
    const int h = allocation.get_height();

    // TODO; temporarily disabled for testing
    // g_object_set(spectrum, "bands", w, nullptr);

    cr->select_font_face("Sans", Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_NORMAL);
    cr->set_font_size(9.0);
    draw_grid(w, h, cr);

    // auto& [ar, ag, ab] = colors["accent"];
    // cr->set_source_rgb(ar, ag, ab);

    // cr->arc(w / 2, h / 2, 8, 0, 2 * M_PI);
    // cr->fill();

    return true;
}

void FrequencyResponsePlot::draw_grid(int w, int h, const Cairo::RefPtr<Cairo::Context>& cr) {
    auto line_range = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    Gdk::RGBA c, b;
    c = colors["background"];
    cr->set_source_rgb(c.get_red(), c.get_green(), c.get_blue());
    cr->rectangle(0, 0, w, h);
    cr->fill();

    c = colors["grid-lines"];
    b = colors["grid-lines-bright"];
    cr->set_line_width(1.0);

    auto m = w / log10(nyquist / start_freq);
    double i, j;
    for(i = 1, j = start_freq; j < nyquist; i += 1.0, j = pow(10.0, i)) {
        for (auto& p : line_range) {
            if (i == 1 && p == 1) continue;
            auto x = floor(m * log10(p * j / start_freq)) + 0.5;
            if (p == 1) {
                cr->set_source_rgb(b.get_red(), b.get_green(), b.get_blue());
            } else {
                cr->set_source_rgb(c.get_red(), c.get_green(), c.get_blue());
            }
            cr->move_to(x, 0);
            cr->line_to(x, h);
            cr->stroke();
        }
    }

    cr->set_source_rgb(1, 1, 1); // white
    for(auto& p : freq_text) {
        auto text = std::get<0>(p);
        auto val = std::get<1>(p);
        auto x = m * log10(val / start_freq);
        cr->move_to(floor(x) + 4.5, h - 2.5);
        cr->show_text(text);
    }

    for(auto db = -15.0; db < 20.0; db += 5.0) {
        auto y_from_db = (0.5 * h) - ((0.5 * h) / 20.0) * db;
        auto y = floor(y_from_db) + 0.5;
        std::ostringstream s;
        s << std::fixed << std::setprecision(0) << db;
        if (db == 0.0) {
            cr->set_source_rgb(b.get_red(), b.get_green(), b.get_blue());
        } else {
            cr->set_source_rgb(c.get_red(), c.get_green(), c.get_blue());
        }
        cr->move_to(0, y);
        cr->line_to(w, y);
        cr->stroke();
        cr->move_to(6.5, y - 1.5);
        cr->set_source_rgb(1, 1, 1); // white
        cr->show_text(s.str());
    }
}
