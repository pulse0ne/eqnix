#ifndef EQUALIZER_HPP
#define EQUALIZER_HPP

#include <gst/gst.h>
#include <vector>
#include <sigc++/sigc++.h>
#include "filter_info.hpp"

class Equalizer {
public:
    Equalizer();
    ~Equalizer();

    GstElement *bin, *id_in, *id_out;
    std::vector<GstElement*> nodes;

    sigc::signal<void, std::shared_ptr<FilterInfo>> filter_updated;
};

#endif // EQUALIZER_HPP
