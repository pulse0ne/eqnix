#include "fr_plot.hpp"
#include <algorithm>
#include <cmath>
#include <complex>
#include <iomanip>
#include "util.hpp"

#define LINE_RANGE {1, 2, 3, 4, 5, 6, 7, 8, 9}
#define DB_SCALE 20.0f
#define HANDLE_RADIUS 7.0f
#define HANDLE_CIRCUMFERENCE 2.0f * HANDLE_RADIUS

FrequencyResponsePlot::FrequencyResponsePlot(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& builder, Colormap _colors, std::shared_ptr<Equalizer> _equalizer)
    : Gtk::DrawingArea(cobject), start_freq(10.0), equalizer(_equalizer), colors(_colors) {
    add_events(Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK | Gdk::SCROLL_MASK);

    equalizer->filter_updated.connect(sigc::mem_fun(*this, &FrequencyResponsePlot::handle_coefficient_update));
}

FrequencyResponsePlot::~FrequencyResponsePlot() {
    //
}

void FrequencyResponsePlot::handle_coefficient_update(std::shared_ptr<FilterInfo> update) {
    logger.debug("coefficients updated for " + update->band);
    filters.insert_or_assign(update->band, update);
    if (samplerate != update->rate) {
        samplerate = update->rate;
    }
    queue_draw();
}

bool FrequencyResponsePlot::on_draw(const CairoCtx& cr) {
    logger.debug("(re)drawing FR plot");
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
    Gdk::RGBA c, b;
    c = colors["background"];
    cr->set_source_rgb(c.get_red(), c.get_green(), c.get_blue());
    cr->rectangle(0, 0, w, h);
    cr->fill();

    c = colors["grid-lines"];
    b = colors["grid-lines-bright"];
    cr->set_line_width(1.0);

    auto nyquist = samplerate / 2.0;
    auto m = w / log10(nyquist / start_freq);
    double i, j;
    for(i = 1, j = start_freq; j < nyquist; i += 1.0, j = pow(10.0, i)) {
        for (auto& p : LINE_RANGE) {
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
        auto y_from_db = (0.5 * h) - ((0.5 * h) / DB_SCALE) * db;
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
    if (filters.empty()) {
        auto c = colors["fr-line"];
        cr->set_source_rgb(c.get_red(), c.get_green(), c.get_blue());
        cr->set_line_width(2.0);
        cr->move_to(0.0, h * 0.5);
        cr->line_to(w, h * 0.5);
        cr->stroke();
        return;
    }

    std::vector<float> freqs(w);
    std::vector<float> mag_res(w);

    auto m = static_cast<float>(w) / log10((samplerate / 2.0) / start_freq);

    for (auto i = 0; i < w; ++i) {
        freqs[i] = pow(10.0, (i / m)) * start_freq;
        mag_res[i] = 1.0;
    }

    auto e = colors["fr-line"];
    cr->set_source_rgba(e.get_red(), e.get_green() + 0.03, e.get_blue(), 0.5);
    cr->set_line_width(1.0);

    for (auto pair : filters) {
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

            auto db_res = DB_SCALE * log10(res);
            auto y = (0.5 * h) * (1 - db_res / DB_SCALE);
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
        auto db_response = DB_SCALE * log10(res);
        auto y = (0.5 * h) * (1 - db_response / DB_SCALE);
        if (x == 0) {
            cr->move_to(x, y);
        } else {
            cr->line_to(x, y);
        }
    }
    cr->stroke();
}

void FrequencyResponsePlot::draw_handles(int w, int h, const CairoCtx& cr) {
    if (filters.empty()) {
        return;
    }

    cr->select_font_face("Sans", Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_BOLD);
    cr->set_font_size(10.0);
    Cairo::TextExtents te;

    auto m = static_cast<float>(w) / log10((samplerate / 2.0) / start_freq);
    int band = 1;
    for (auto pair : filters) {
        auto f = pair.second;
        auto x = floorf(m * log10(f->freq / start_freq));
        auto res = frequency_response(f->freq, samplerate, f->b0, f->b1, f->b2, f->a1, f->a2);
        float y = (0.5 * h) * (1 - log10(res));
        y = std::min(h + HANDLE_RADIUS, std::max(HANDLE_RADIUS, y)) - HANDLE_RADIUS;

        locations.insert_or_assign(f, std::make_pair<double, double>(x, y));

        auto c = colors["accent"];
        auto bt = std::to_string(band++);

        cr->set_line_width(1.5);

        if (f == selected_filter) {
            cr->set_source_rgb(c.get_red(), c.get_green(), c.get_blue());
            cr->arc(x, y, HANDLE_RADIUS, 0, 2 * M_PI);
            cr->fill();
        } else {
            cr->set_source_rgba(0, 0, 0, 0.5);
            cr->arc(x, y, HANDLE_RADIUS, 0, 2 * M_PI);
            cr->fill();
        }

        cr->set_source_rgb(c.get_red(), c.get_green(), c.get_blue());
        cr->arc(x, y, HANDLE_RADIUS, 0, 2 * M_PI);
        cr->stroke();

        if (f == selected_filter) {
            cr->set_source_rgb(0, 0, 0);
        }
        cr->get_text_extents(bt, te);
        cr->move_to(x - te.x_bearing - te.width / 2, y + HANDLE_RADIUS * 0.5);
        cr->show_text(bt);
        cr->stroke();
    }
}

bool FrequencyResponsePlot::on_button_press_event(GdkEventButton* ev) {
    is_dragging = true;
    for (auto entry : locations) {
        auto xy = entry.second;
        auto x = xy.first;
        auto y = xy.second;
        auto x_hit = x + HANDLE_RADIUS;
        auto y_hit = y + HANDLE_RADIUS;
        auto diff_x = x_hit - ev->x;
        auto diff_y = y_hit - ev->y;

        auto hit = diff_x > 0 && diff_y > 0 && diff_x < HANDLE_CIRCUMFERENCE && diff_y < HANDLE_CIRCUMFERENCE;
        if (hit) {
            selected_filter = entry.first;
            queue_draw();
            break;
        }
    }
    return true;
}

bool FrequencyResponsePlot::on_button_release_event(GdkEventButton* ev) {
    is_dragging = false;
    return true;
}

static inline double x_to_freq(double x, double w, uint samplerate, double start_freq) {
    auto m = static_cast<double>(w) / log10(static_cast<double>(samplerate) / 2.0 / start_freq);
    return pow(10.0, x / m) * start_freq;
}

bool FrequencyResponsePlot::on_motion_notify_event(GdkEventMotion* ev) {
    auto bounds = get_allocation();
    auto x = ev->x;
    auto y = ev->y;

    if (!is_dragging || !selected_filter) {
        return true;
    }

    if (selected_filter->filtertype - 3 < 0) {
        auto gain = DB_SCALE * (((-2 * y) / bounds.get_height()) + 1.0);
        if (abs(gain) <= 24.0) {
            selected_filter->gain = gain;
        }
    }

    auto freq = x_to_freq(x, bounds.get_width(), samplerate, start_freq);
    selected_filter->freq = freq;

    equalizer->filter_changed.emit(selected_filter);
    return true;
}

bool FrequencyResponsePlot::on_scroll_event(GdkEventScroll* ev) {
    if (selected_filter) {
        g_print("scroll\n");
    }

    return true;
}
