#include "util.hpp"

namespace util {

gdouble calculate_omega(gdouble freq, gint rate) {
    gdouble omega;

    if (freq / rate >= 0.5)
        omega = G_PI;
    else if (freq <= 0.0)
        omega = 0.0;
    else
        omega = 2.0 * G_PI * (freq / rate);

    return omega;
}

}  // namespace util
