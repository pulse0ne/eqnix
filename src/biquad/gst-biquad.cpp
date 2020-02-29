#include "gst-biquad.hpp"
#include "config.h"

#include <string>
#include <cmath>
#include <vector>

#include <gst/glib-compat.h>

#define ALLOWED_CAPS \
    "audio/x-raw,"                                                      \
    " format=(string) {" GST_AUDIO_NE(F32) "," GST_AUDIO_NE(F64) " }, " \
    " rate=(int)[1000,MAX],"                                            \
    " channels=(int)[1,MAX],"                                           \
    " layout=(string)interleaved"


#define GST_TYPE_FILTER_TYPE (gst_biquad_filter_type_get_type())

GST_DEBUG_CATEGORY_STATIC(biquad_debug);
#define GST_CAT_DEFAULT (biquad_debug)

static GType gst_biquad_filter_type_get_type() {
    static GType gtype = 0;
    if (gtype == 0) {
        static const GEnumValue values[] = {
            {BiquadFilterType::ALLPASS, "Allpass filter", "allpass"},
            {BiquadFilterType::BANDPASS, "Bandpass filter", "bandpass"},
            {BiquadFilterType::HIGHPASS, "Highpass filter", "highpass"},
            {BiquadFilterType::HIGHSHELF, "Highshelf filter", "highshelf"},
            {BiquadFilterType::LOWPASS, "Lowpass filter", "lowpass"},
            {BiquadFilterType::LOWSHELF, "Lowshelf filter", "lowshelf"},
            {BiquadFilterType::NOTCH, "Notch filter", "notch"},
            {BiquadFilterType::PEAKING, "Peaking filter", "peaking"}
        };
        gtype = g_enum_register_static("BiquadFilterType", values);
    }
    return gtype;
}

GstAudioFilterClass* GstBiquad::s_parent_class = nullptr;

GType GstBiquad::get_type() {
    static volatile gsize gonce_data = 0;
    if (g_once_init_enter(&gonce_data)) {
        GType type = 0;
        GTypeInfo info;
        info.class_size = sizeof(GstBiquadClass);
        info.base_init = &GstBiquad::base_init;
        info.base_finalize = NULL;
        info.class_init = &GstBiquad::class_init;
        info.class_finalize = NULL;
        info.class_data = NULL;
        info.instance_size = sizeof(GstBiquad);
        info.n_preallocs = 0;
        info.instance_init = &GstBiquad::init;
        info.value_table = 0;
        type = g_type_register_static(GST_TYPE_AUDIO_FILTER, g_intern_static_string("biquad"), &info, (GTypeFlags)0);
        g_once_init_leave(&gonce_data, (gsize) type);
    }
    return (GType) gonce_data;
}

void GstBiquad::base_init(gpointer g_class) {
    GstAudioFilterClass* afc = GST_AUDIO_FILTER_CLASS(g_class);
    GstCaps* caps = gst_caps_from_string(ALLOWED_CAPS);
    gst_audio_filter_class_add_pad_templates(afc, caps);
    gst_caps_unref(caps);
}

void GstBiquad::class_init(gpointer g_class, gpointer class_data) {
    s_parent_class = reinterpret_cast<GstAudioFilterClass*>(g_type_class_peek_parent(g_class));

    GObjectClass* obj_class = G_OBJECT_CLASS(g_class);
    obj_class->finalize = GstBiquad::finalize;
    obj_class->set_property = GstBiquad::set_property;
    obj_class->get_property = GstBiquad::get_property;

    GstBaseTransformClass* bt_class = GST_BASE_TRANSFORM_CLASS(g_class);
    bt_class->transform_ip = GstBiquad::transform_ip;
    bt_class->transform_ip_on_passthrough = FALSE;

    GstAudioFilterClass* afc = GST_AUDIO_FILTER_CLASS(g_class);
    afc->setup = GstBiquad::setup;

    GParamFlags flags = (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_CONTROLLABLE);
    g_object_class_install_property(obj_class, PROP_FREQUENCY, 
        g_param_spec_double("frequency", "frequency", "center freq", 0.0, 24000.0, 350.0, flags));
    
    g_object_class_install_property(obj_class, PROP_Q,
        g_param_spec_double("q", "Q", "The Q (or resonance)", 0.0001, 1000.0, 1.0, flags));

    g_object_class_install_property(obj_class, PROP_GAIN,
        g_param_spec_double("gain", "gain", "Boost or cut to apply", -40.0, 40.0, 0.0, flags));

    g_object_class_install_property(obj_class, PROP_FILTER_TYPE,
        g_param_spec_enum("filtertype", "filtertype", "Type of filter", gst_biquad_filter_type_get_type(), BiquadFilterType::PEAKING, flags));
    
    g_object_class_install_property(obj_class, PROP_FR_NUM_BANDS,
        g_param_spec_uint("frbands", "frbands", "Number of bands for frequency response", 1, G_MAXUINT32, 128, flags));
    
    g_object_class_install_property(obj_class, PROP_EMIT_FR,
        g_param_spec_boolean("emitfr", "emitfr", "Whether to emit FR information", FALSE, flags));
    
}

gboolean GstBiquad::setup(GstAudioFilter* audio, const GstAudioInfo* info) {
    GstBiquad* b = GST_BIQUAD(audio);
    switch (GST_AUDIO_INFO_FORMAT(info)) {
        case GST_AUDIO_FORMAT_F32:
            b->isdoublewide = false;
            break;
        case GST_AUDIO_FORMAT_F64:
            b->isdoublewide = true;
            break;
        default:
            return FALSE;
    }
    return TRUE;
}

void GstBiquad::init(GTypeInstance* instance, gpointer g_class) {
    GstBiquad* b = GST_BIQUAD(instance);
    b->delegate = new Biquad();
}

void GstBiquad::finalize(GObject* object) {
    GstBiquad* b = GST_BIQUAD(object);
    delete b->delegate;
    b->delegate = 0;
}

void GstBiquad::set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec) {
    GstBiquad* b = GST_BIQUAD(object);
    bool fr_needs_update = false;
    switch (prop_id) {
        case PROP_FILTER_TYPE: {
            BiquadFilterType ft = (BiquadFilterType) g_value_get_enum(value);
            if (b->delegate->get_filter_type() != ft) {
                b->delegate->set_filter_type(ft);
                fr_needs_update = true;
            }
            break;
        }
        case PROP_FREQUENCY: {
            double f = g_value_get_double(value);
            if (b->delegate->get_frequency() != f) {
                b->delegate->set_frequency(f);
                fr_needs_update = true;
            }
            break;
        }
        case PROP_Q: {
            double q = g_value_get_double(value);
            if (b->delegate->get_q() != q) {
                b->delegate->set_q(q);
                fr_needs_update = true;
            }
            break;
        }
        case PROP_GAIN: {
            double g = g_value_get_double(value);
            if (b->delegate->get_gain() != g) {
                b->delegate->set_gain(g);
                fr_needs_update = true;
            }
            break;
        }
        case PROP_FR_NUM_BANDS:
            b->num_fr_bands = g_value_get_uint(value);
            fr_needs_update = true;
            break;
        case PROP_EMIT_FR:
            b->emit_fr = g_value_get_boolean(value);
            fr_needs_update = b->emit_fr;
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }

    if (fr_needs_update && b->emit_fr) {
        GstElement* el = GST_ELEMENT(object);
        GstMessage* msg = b->create_fr_message();
        gst_element_post_message(el, msg);
    }
}

void GstBiquad::get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec) {
    GstBiquad* b = GST_BIQUAD(object);
    switch (prop_id) {
        case PROP_FILTER_TYPE:
            g_value_set_enum(value, b->delegate->get_filter_type());
            break;
        case PROP_FREQUENCY:
            g_value_set_double(value, b->delegate->get_frequency());
            break;
        case PROP_Q:
            g_value_set_double(value, b->delegate->get_q());
            break;
        case PROP_GAIN:
            g_value_set_double(value, b->delegate->get_gain());
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

GstFlowReturn GstBiquad::transform_ip(GstBaseTransform* trans, GstBuffer* in) {
    GstAudioFilter* filter = GST_AUDIO_FILTER(trans);
    GstBiquad* biquad = GST_BIQUAD(trans);
    GstMapInfo map;
    GstClockTime timestamp;
    guint channels = GST_AUDIO_FILTER_CHANNELS(filter);

    if (G_UNLIKELY(channels < 1))
        return GST_FLOW_NOT_NEGOTIATED;

    timestamp = GST_BUFFER_TIMESTAMP(in);
    timestamp = gst_segment_to_stream_time(&trans->segment, GST_FORMAT_TIME, timestamp);

    if (GST_CLOCK_TIME_IS_VALID(timestamp)) {
        gst_object_sync_values(GST_OBJECT(biquad), timestamp);
    }

    GstMapFlags flags = static_cast<GstMapFlags>(GST_MAP_READWRITE);
    gst_buffer_map(in, &map, flags);
    guint8* data = map.data;
    int frames;
    if (biquad->isdoublewide) {
        frames = map.size / channels / sizeof(double);
        double cur;
        for (auto f = 0; f < frames; f++) {
            for (guint c = 0; c < channels; c++) {
                cur = *((double*) data);
                cur = biquad->delegate->process(cur);
            }
            *((double*) data) = cur;
            data += sizeof(double);
        }
    } else {
        frames = map.size / channels / sizeof(float);
        float cur;
        for (auto f = 0; f < frames; f++) {
            for (guint c = 0; c < channels; c++) {
                cur = *((float*) data);
                cur = biquad->delegate->process(cur);
            }
            *((float*) data) = cur;
            data += sizeof(float);
        }
    }
    gst_buffer_unmap(in, &map);
    return GST_FLOW_OK;
}

static GValue* message_add_container(GstStructure* s, GType type, const gchar* name) {
    GValue v = { 0, };
    g_value_init(&v, type);
    gst_structure_set_value(s, name, &v);
    g_value_unset(&v);
    return (GValue*) gst_structure_get_value(s, name);
}

static void message_add_array(GValue* cv, std::vector<double> data, guint n) {
    GValue v = { 0, };
    GValue a = { 0, };

    g_value_init (&a, GST_TYPE_ARRAY);
    g_value_init (&v, G_TYPE_FLOAT);
    for (guint i = 0; i < n; i++) {
        g_value_set_float (&v, data[i]);
        gst_value_array_append_value (&a, &v);
    }
    g_value_unset (&v);

    gst_value_array_append_value (cv, &a);
    g_value_unset (&a);
}

GstMessage* GstBiquad::create_fr_message() {
    Biquad* b = delegate;
    std::vector<double> frequencies(num_fr_bands);
    std::vector<double> mag_res(num_fr_bands);
    std::vector<double> phase_res(num_fr_bands);

    auto rate = GST_AUDIO_FILTER_RATE(this);
    auto m = static_cast<double>(num_fr_bands) / log10(rate / 2.0);
    for (guint i = 0; i < num_fr_bands; i++) {
        frequencies[i] = pow(10.0, i / m);
    }

    b->get_frequency_response(num_fr_bands, &frequencies[0], &mag_res[0], &phase_res[0]);

    GstStructure* s = gst_structure_new("frequency-response", "nfreqs", G_TYPE_UINT, num_fr_bands, nullptr);

    GValue *mcv = message_add_container(s, GST_TYPE_ARRAY, "magnitude");
    message_add_array(mcv, mag_res, num_fr_bands);

    GValue *pcv = message_add_container(s, GST_TYPE_ARRAY, "phase");
    message_add_array(pcv, phase_res, num_fr_bands);

    return gst_message_new_element(GST_OBJECT(this), s);
}

static gboolean plugin_init(GstPlugin* plugin) {
    GST_DEBUG_CATEGORY_INIT(biquad_debug, "eqnix", 0, "Debug category for GstQtVideoSink");

    if (!(gst_element_register(plugin, "biquad", GST_RANK_NONE, GST_TYPE_BIQUAD))) {
        return FALSE;
    }
    return TRUE;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, biquad, "Biquad filter", plugin_init, VERSION, "MIT", PACKAGE, "https://github.com/pulse0ne/eqnix")
