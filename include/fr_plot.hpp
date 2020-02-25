#ifndef FR_PLOT_HPP
#define FR_PLOT_HPP

#include <gtkmm.h>
#include <cairomm/cairomm.h>
#include <map>

using Colormap = std::map<std::string, Gdk::RGBA>;

class FrequencyResponsePlot : public Gtk::DrawingArea {
public:
    FrequencyResponsePlot(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& builder, Colormap colormap);
    ~FrequencyResponsePlot();

private:
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override;

    void draw_grid(int w, int h, const Cairo::RefPtr<Cairo::Context>& cr);

    std::map<std::string, const double> freq_text = {
        {"100", 100.0},
        {"1k", 1000.0},
        {"10k", 10000.0}
    };

    Colormap colors;
    double nyquist;
    double start_freq;
};

#endif // FR_PLOT_HPP
