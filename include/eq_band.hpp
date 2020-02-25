#ifndef EQ_BAND_HPP
#define EQ_BAND_HPP

#include <glib/gtypes.h>

class EQBand {
public:
    gfloat freq, q, gain;
    bool enabled;
};

#endif // EQ_BAND_HPP
