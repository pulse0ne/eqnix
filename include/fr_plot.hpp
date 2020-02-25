#ifndef FR_PLOT_HPP
#define FR_PLOT_HPP

#include <gst/gst.h>
#include <gtkmm.h>
#include <cairomm/cairomm.h>
#include <map>
#include <memory>
#include "equalizer.hpp"

using Colormap = std::map<std::string, Gdk::RGBA>;

class FrequencyResponsePlot : public Gtk::DrawingArea {
public:
    FrequencyResponsePlot(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& builder, Colormap colormap, std::shared_ptr<Equalizer> _equalizer);
    ~FrequencyResponsePlot();

    double nyquist;
    double start_freq;

    std::shared_ptr<Equalizer> equalizer;

    GstElement *pipeline, *src, *spectrum, *sink;
    GstBus *bus;

private:
    std::map<std::string, const double> freq_text = {
        {"100", 100.0},
        {"1k", 1000.0},
        {"10k", 10000.0}
    };

    Colormap colors;

    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override;
    void draw_grid(int w, int h, const Cairo::RefPtr<Cairo::Context>& cr);
};

#endif // FR_PLOT_HPP
