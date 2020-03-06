#include "filter_info.hpp"

FilterInfo::FilterInfo() {
}

std::shared_ptr<FilterInfo> FilterInfo::from_structure(const GstStructure* s) {
    struct builder : public FilterInfo {};
    std::shared_ptr<FilterInfo> cu = std::make_shared<builder>();
    gst_structure_get_double(s, "b0", &cu->b0);
    gst_structure_get_double(s, "b1", &cu->b1);
    gst_structure_get_double(s, "b2", &cu->b2);
    gst_structure_get_double(s, "a1", &cu->a1);
    gst_structure_get_double(s, "a2", &cu->a2);
    gst_structure_get_uint(s, "rate", &cu->rate);
    gst_structure_get_int(s, "type", &cu->filtertype);
    gst_structure_get(s,
        "b0", G_TYPE_DOUBLE, &cu->b0,
        "b1", G_TYPE_DOUBLE, &cu->b1,
        "b2", G_TYPE_DOUBLE, &cu->b2,
        "a1", G_TYPE_DOUBLE, &cu->a1,
        "a2", G_TYPE_DOUBLE, &cu->a2,
        "gain", G_TYPE_DOUBLE, &cu->gain,
        "freq", G_TYPE_DOUBLE, &cu->freq,
        "q", G_TYPE_DOUBLE, &cu->q,
        "rate", G_TYPE_UINT, &cu->rate,
        "type", G_TYPE_INT, &cu->filtertype,
        nullptr);
    cu->band = std::string(gst_structure_get_string(s, "band"));
    return cu;
}