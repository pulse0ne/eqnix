#ifndef FR_PLOT_HPP
#define FR_PLOT_HPP

#include <gtkmm.h>
#include <cairomm/cairomm.h>
#include <tuple>
#include <map>

using tuplemap = std::map<std::string, std::tuple<double, double, double>>;

class FrequencyResponsePlot : public Gtk::DrawingArea {
public:
    FrequencyResponsePlot(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& builder, tuplemap _colors);
    ~FrequencyResponsePlot();

private:
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override;

    void draw_grid(int w, int h, const Cairo::RefPtr<Cairo::Context>& cr);

    std::map<std::string, const double> freq_text = {
        {"100", 100.0},
        {"1k", 1000.0},
        {"10k", 10000.0}
    };

    tuplemap colors;
    double nyquist;
    double start_freq;
};

#endif // FR_PLOT_HPP
