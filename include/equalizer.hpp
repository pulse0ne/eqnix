#ifndef EQUALIZER_HPP
#define EQUALIZER_HPP

#include <gst/gst.h>
#include "eq_band.hpp"

class Equalizer {
public:
    Equalizer();
    ~Equalizer();

    EQBand bands[8]{};

    GstElement *eq;
};

#endif // EQUALIZER_HPP
