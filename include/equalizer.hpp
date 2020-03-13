#ifndef EQUALIZER_HPP
#define EQUALIZER_HPP

#include <gst/gst.h>
#include <vector>
#include <sigc++/sigc++.h>
#include "filter_info.hpp"

enum FilterChangeType {
    FREQ, GAIN, Q, TYPE
};

class Equalizer {
public:
    Equalizer();
    ~Equalizer();

    GstElement *bin, *id_in, *id_out;
    std::vector<GstElement*> nodes;

    sigc::signal<void, std::shared_ptr<FilterInfo>> filter_updated;
    sigc::signal<void, std::string, FilterChangeType, const GValue*> change_filter;

private:
    void handle_change_filter(std::string name, FilterChangeType change_type, const GValue* value);
};

#endif // EQUALIZER_HPP
