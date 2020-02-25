#include "fr_plot.hpp"
#include <cmath>
#include <iomanip>
#include "util.hpp"

FrequencyResponsePlot::FrequencyResponsePlot(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& builder, tuplemap _colors)
    : Gtk::DrawingArea(cobject), colors(_colors), nyquist(22050.0), start_freq(10.0) {
    add_events(Gdk::BUTTON_PRESS_MASK);
}

FrequencyResponsePlot::~FrequencyResponsePlot() {}

bool FrequencyResponsePlot::on_draw(const Cairo::RefPtr<Cairo::Context>& cr) {
    Gtk::Allocation allocation = get_allocation();
    const int w = allocation.get_width();
    const int h = allocation.get_height();

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
    double r, g, b;
    std::tie(r, g, b) = colors["background"];
    cr->set_source_rgb(r, g, b);
    cr->rectangle(0, 0, w, h);
    cr->fill();

    std::tie(r, g, b) = colors["grid-lines"];
    auto& [rb, gb, bb] = colors["grid-lines-bright"];
    cr->set_line_width(1.0);

    auto m = w / log10(nyquist / start_freq);
    double i, j;
    for(i = 1, j = start_freq; j < nyquist; i += 1.0, j = pow(10.0, i)) {
        for (auto& p : line_range) {
            if (i == 1 && p == 1) continue;
            auto x = floor(m * log10(p * j / start_freq)) + 0.5;
            if (p == 1) {
                cr->set_source_rgb(rb, gb, bb);
            } else {
                cr->set_source_rgb(r, g, b);
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
            cr->set_source_rgb(rb, gb, bb);
        } else {
            cr->set_source_rgb(r, g, b);
        }
        cr->move_to(0, y);
        cr->line_to(w, y);
        cr->stroke();
        cr->move_to(6.5, y - 1.5);
        cr->set_source_rgb(1, 1, 1); // white
        cr->show_text(s.str());
    }
}
