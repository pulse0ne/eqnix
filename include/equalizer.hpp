#ifndef EQUALIZER_HPP
#define EQUALIZER_HPP

#include <gst/gst.h>
#include <vector>

class Equalizer {
public:
    Equalizer();
    ~Equalizer();

    GstElement *bin;
    std::vector<GstElement*> nodes;
};

#endif // EQUALIZER_HPP
