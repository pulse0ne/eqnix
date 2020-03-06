#ifndef __FILTER_COEFFICIENTS_HPP__
#define __FILTER_COEFFICIENTS_HPP__

#include <string>
#include <gst/gst.h>
#include <memory>

class FilterInfo {
public:
    gdouble b0, b1, b2, a1, a2;
    gdouble freq, gain, q;
    gint filtertype;
    guint rate;
    std::string band;

    static std::shared_ptr<FilterInfo> from_structure(const GstStructure* s);
private:
    FilterInfo();
};

#endif // __FILTER_COEFFICIENTS_HPP__
