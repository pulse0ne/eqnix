#include "fr_plot.hpp"
#include <algorithm>
#include <cmath>
#include <complex>
#include <iomanip>
#include "util.hpp"

FrequencyResponsePlot::FrequencyResponsePlot(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& builder, Colormap _colors, std::shared_ptr<Equalizer> _equalizer)
    : Gtk::DrawingArea(cobject), nyquist(22050.0), start_freq(10.0), equalizer(_equalizer), colors(_colors) {
    add_events(Gdk::BUTTON_PRESS_MASK);

    equalizer->filter_updated.connect(sigc::mem_fun(*this, &FrequencyResponsePlot::handle_coefficient_update));
}

FrequencyResponsePlot::~FrequencyResponsePlot() {
    //
}

void FrequencyResponsePlot::handle_coefficient_update(std::shared_ptr<FilterInfo> update) {
    g_print("%s coefficients updated\n", update->band.c_str());
    coefficients.insert_or_assign(update->band, update);
    if (samplerate != update->rate) {
        samplerate = update->rate;
    }
    queue_draw();
}

bool FrequencyResponsePlot::on_draw(const CairoCtx& cr) {
    g_print("drawing\n");
    Gtk::Allocation allocation = get_allocation();
    const int w = allocation.get_width();
    const int h = allocation.get_height();

    cr->select_font_face("Sans", Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_NORMAL);
    cr->set_font_size(9.0);
    draw_grid(w, h, cr);
    draw_lines(w, h, cr);
    draw_handles(w, h, cr);

    return true;
}

void FrequencyResponsePlot::draw_grid(int w, int h, const CairoCtx& cr) {
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

static inline float frequency_response(float freq, uint samplerate, gdouble b0, gdouble b1, gdouble b2, gdouble a1, gdouble a2) {
    double omega = util::calculate_omega(freq, samplerate);
    std::complex<double> z = std::complex(cos(omega), sin(omega));
    std::complex<double> numer = b0 + (b1 + b2*z) * z;
    std::complex<double> denom = std::complex<double>(1, 0) + (a1 + a2*z) * z;
    return static_cast<float>(fabs(numer / denom));
}

void FrequencyResponsePlot::draw_lines(int w, int h, const CairoCtx& cr) {
    if (coefficients.empty()) {
        auto c = colors["fr-line"];
        cr->set_source_rgb(c.get_red(), c.get_green(), c.get_blue());
        cr->set_line_width(2.0);
        cr->move_to(0.0, h * 0.5);
        cr->line_to(w, h * 0.5);
        cr->stroke();
        return;
    }

    const float freq_sart = 10.0;

    std::vector<float> freqs(w);
    std::vector<float> mag_res(w);

    auto m = static_cast<float>(w) / log10((samplerate / 2.0) / freq_sart);

    for (auto i = 0; i < w; ++i) {
        freqs[i] = pow(10.0, (i / m)) * freq_sart;
        mag_res[i] = 1.0;
    }

    auto e = colors["fr-line"];
    cr->set_source_rgba(e.get_red(), e.get_green() + 0.03, e.get_blue(), 0.5);
    cr->set_line_width(1.0);

    for (auto pair : coefficients) {
        auto coeff = pair.second;
        auto b0 = coeff->b0;
        auto b1 = coeff->b1;
        auto b2 = coeff->b2;
        auto a1 = coeff->a1;
        auto a2 = coeff->a2;

        for (auto i = 0; i < w; ++i) {
            double f = freqs[i];
            float res = frequency_response(f, samplerate, b0, b1, b2, a1, a2);
            mag_res[i] *= res;

            auto db_res = 20.0 * log10(res);
            auto y = (0.5 * h) * (1 - db_res / 20.0);
            if (i == 0) {
                cr->move_to(i, y);
            } else {
                cr->line_to(i, y);
            }
        }
        cr->stroke();
    }

    auto c = colors["fr-line"];
    cr->set_source_rgb(c.get_red(), c.get_green(), c.get_blue());
    cr->set_line_width(2.0);
    for (auto x = 0; x < w; ++x) {
        auto res = mag_res[x];
        auto db_response = 20.0 * log10(res);
        auto y = (0.5 * h) * (1 - db_response / 20.0);
        if (x == 0) {
            cr->move_to(x, y);
        } else {
            cr->line_to(x, y);
        }
    }
    cr->stroke();
}

void FrequencyResponsePlot::draw_handles(int w, int h, const CairoCtx& cr) {
    if (coefficients.empty()) {
        return;
    }

    float freq_start = 10.0;
    auto m = static_cast<float>(w) / log10((samplerate / 2.0) / freq_start);
    for (auto pair : coefficients) {
        auto f = pair.second;
        auto x = floorf(m * log10(f->freq / freq_start));
        auto res = frequency_response(f->freq, samplerate, f->b0, f->b1, f->b2, f->a1, f->a2);
        float y = (0.5 * h) * (1 - log10(res));
        y = std::min(h + 10.0f, std::max(10.0f, y)) - 10;

        auto c = colors["accent"];
        cr->set_source_rgb(c.get_red(), c.get_green(), c.get_blue());
        cr->set_line_width(2.0);
        cr->arc(x, y, 8.0, 0, 2 * M_PI);
        cr->stroke();
    }
}
