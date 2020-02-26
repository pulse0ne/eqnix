#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "magresponse.h"
#include <math.h>
#include <string.h>

GST_DEBUG_CATEGORY_STATIC(gst_magresponse_debug);
#define GST_CAT_DEFAULT gst_magresponse_debug;

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define FORMATS "{ S16LE, S24LE, S32LE, F32LE, F64LE }"
#else
#define FORMATS "{ S16BE, S24BE, S32BE, F32BE, F64BE }"
#endif

#define ALLOWED_CAPS GST_AUDIO_CAPS_MAKE(FORMATS) ", layout = (string) interleaved"

// Magresponse properties
#define DEFAULT_INTERVAL (GST_SECOND / 10)
#define DEFAULT_BANDS 256
#define DEFAULT_MULTI_CHANNEL FALSE

enum { PROP_0, PROP_INTERVAL, PROP_BANDS, PROP_MULTI_CHANNEL };

#define gst_magresponse_parent_class parent_class
G_DEFINE_TYPE(GstMagresponse, gst_magresponse, GST_TYPE_AUDIO_FILTER);

static void gst_magresponse_finalize(GObject* object);
static void gst_magresponse_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec);
static void gst_magresponse_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec);
static gboolean gst_magresponse_start(GstBaseTransform* trans);
static gboolean gst_magresponse_stop(GstBaseTransform* trans);
static GstFlowReturn gst_magresponse_transform_ip(GstBaseTransform* trans, GstBuffer* in);
static gboolean gst_magresponse_setup(GstAudioFilter* base, const GstAudioInfo* info);

static void gst_magresponse_class_init(GstMagresponseClass* klass) {
    GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
    GstElementClass* element_class = GST_ELEMENT_CLASS(klass);
    GstBaseTransformClass* trans_class = GST_BASE_TRANSFORM_CLASS(klass);
    GstAudioFilterClass* filter_class = GST_AUDIO_FILTER_CLASS(klass);
    GstCaps* caps;

    gobject_class->set_property = gst_magresponse_set_property;
    gobject_class->get_property = gst_magresponse_get_property;
    gobject_class->finalize = gst_magresponse_finalize;

    trans_class->start = GST_DEBUG_FUNCPTR(gst_magresponse_start);
    trans_class->stop = GST_DEBUG_FUNCPTR(gst_magresponse_stop);
    trans_class->transform_ip = GST_DEBUG_FUNCPTR(gst_magresponse_transform_ip);
    trans_class->passthrough_on_same_caps = TRUE;

    filter_class->setup = GST_DEBUG_FUNCPTR(gst_magresponse_setup);

    g_object_class_install_property(
        gobject_class, PROP_INTERVAL,
        g_param_spec_uint64("interval", "Interval", "Interval between messages in ns", 1, G_MAXUINT64, DEFAULT_INTERVAL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(
        gobject_class, PROP_BANDS,
        g_param_spec_uint("bands", "Bands", "Number of frequency bands", 2, ((guint)G_MAXINT + 2) / 2, DEFAULT_BANDS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(
        gobject_class, PROP_MULTI_CHANNEL,
        g_param_spec_boolean("multi-channel", "Multichannel results", "Send separate results for each channel", DEFAULT_MULTI_CHANNEL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    GST_DEBUG_CATEGORY_INIT(gst_magresponse_debug, "magresponse", 0, "mag response analyzer element");

    gst_element_class_set_static_metadata(element_class, "MagResponse analyzer", "Filter/Analyzer/Audio", "Output mag response", "Tyler Snedigar <snedigart@gmail.com>");

    caps = gst_caps_from_string(ALLOWED_CAPS);
    gst_audio_filter_class_add_pad_templates(filter_class, caps);
    gst_caps_unref(caps);
}

static void gst_magresponse_init(GstMagresponse* mr) {
    mr->interval = DEFAULT_INTERVAL;
    mr->bands = DEFAULT_BANDS;
    g_mutex_init(&mr->lock);
}

static void gst_magresponse_alloc_channel_data(GstMagresponse* mr) {
    gint i;
    GstMagresponseChannel* cd;
    guint bands = mr->bands;
    guint nfft = 2 * bands - 2;

    g_assert(mr->channel_data == NULL);

    mr->num_channels = (mr->multi_channel) ? GST_AUDIO_FILTER_CHANNELS(mr) : 1;

    GST_DEBUG_OBJECT(mr, "allocating data for %d channels", mr->num_channels);

    mr->channel_data = g_new(GstMagresponseChannel, mr->num_channels);
    for (i = 0; i < mr->num_channels; i++) {
        cd = &mr->channel_data[i];
        cd->fft_ctx = gst_fft_f32_new(nfft, FALSE);
        cd->input = g_new0(gfloat, nfft);
        cd->input_tmp = g_new0(gfloat, nfft);
        cd->freqdata = g_new0(GstFFTF32Complex, bands);
        cd->magnitude = g_new0(gfloat, bands);
        cd->phase = g_new0(gfloat, bands);
    }
}

static void gst_magresponse_free_channel_data(GstMagresponse* mr) {
    if (mr->channel_data) {
        gint i;
        GstMagresponseChannel* cd;

        GST_DEBUG_OBJECT(mr, "freeing data for %d channels", mr->num_channels);

        for (i = 0; i < mr->num_channels; i++) {
            cd = &mr->channel_data[i];
            if (cd->fft_ctx) {
                gst_fft_f32_free(cd->fft_ctx);
            }
            g_free(cd->input);
            g_free(cd->input_tmp);
            g_free(cd->freqdata);
            g_free(cd->magnitude);
            g_free(cd->phase);
        }
        g_free(mr->channel_data);
        mr->channel_data = NULL;
    }
}

static void gst_magresponse_flush(GstMagresponse* mr) {
    mr->num_frames = 0;
    mr->num_fft = 0;
    mr->accumulated_error = 0;
}

static void gst_magresponse_reset_state(GstMagresponse* mr) {
    GST_DEBUG_OBJECT(mr, "resetting state");
    gst_magresponse_free_channel_data(mr);
    gst_magresponse_flush(mr);
}

static void gst_magresponse_finalize(GObject* object) {
    GstMagresponse* mr = GST_MAGRESPONSE(object);
    gst_magresponse_reset_state(mr);
    g_mutex_clear(&mr->lock);
    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void gst_magresponse_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec) {
    GstMagresponse* filter = GST_MAGRESPONSE(object);

    switch (prop_id) {
    case PROP_INTERVAL: {
        guint64 interval = g_value_get_uint64(value);
        g_mutex_lock(&filter->lock);
        if (filter->interval != interval) {
            filter->interval = interval;
            gst_magresponse_reset_state(filter);
        }
        g_mutex_unlock(&filter->lock);
        break;
    }
    case PROP_BANDS: {
        guint bands = g_value_get_uint(value);
        g_mutex_lock(&filter->lock);
        if (filter->bands != bands) {
            filter->bands = bands;
            gst_magresponse_reset_state(filter);
        }
        g_mutex_unlock(&filter->lock);
        break;
    }
    case PROP_MULTI_CHANNEL: {
        gboolean multi_channel = g_value_get_boolean(value);
        g_mutex_lock(&filter->lock);
        if (filter->multi_channel != multi_channel) {
            filter->multi_channel = multi_channel;
            gst_magresponse_reset_state(filter);
        }
        g_mutex_unlock(&filter->lock);
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void gst_magresponse_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec) {
    GstMagresponse* filter = GST_MAGRESPONSE(object);

    switch (prop_id) {
    case PROP_INTERVAL:
        g_value_set_uint64(value, filter->interval);
        break;
    case PROP_BANDS:
        g_value_set_uint(value, filter->interval);
        break;
    case PROP_MULTI_CHANNEL:
        g_value_set_boolean(value, filter->multi_channel);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static gboolean gst_magresponse_start(GstBaseTransform* trans) {
    GstMagresponse* mr = GST_MAGRESPONSE(trans);
    gst_magresponse_reset_state(mr);
    return TRUE;
}

static gboolean gst_magresponse_stop(GstBaseTransform* trans) { return gst_magresponse_start(trans); }

/* mixing data readers */
static void input_data_mixed_float(const guint8* _in, gfloat* out, guint len, guint channels, gfloat max_value, guint op, guint nfft) {
    guint i, j, ip = 0;
    gfloat v;
    gfloat* in = (gfloat*)_in;

    for (j = 0; j < len; j++) {
        v = in[ip++];
        for (i = 1; i < channels; i++)
            v += in[ip++];
        out[op] = v / channels;
        op = (op + 1) % nfft;
    }
}

static void input_data_mixed_double(const guint8* _in, gfloat* out, guint len, guint channels, gfloat max_value, guint op, guint nfft) {
    guint i, j, ip = 0;
    gfloat v;
    gdouble* in = (gdouble*)_in;

    for (j = 0; j < len; j++) {
        v = in[ip++];
        for (i = 1; i < channels; i++)
            v += in[ip++];
        out[op] = v / channels;
        op = (op + 1) % nfft;
    }
}

static void input_data_mixed_int32_max(const guint8* _in, gfloat* out, guint len, guint channels, gfloat max_value, guint op, guint nfft) {
    guint i, j, ip = 0;
    gint32* in = (gint32*)_in;
    gfloat v;

    for (j = 0; j < len; j++) {
        v = in[ip++] / max_value;
        for (i = 1; i < channels; i++)
            v += in[ip++] / max_value;
        out[op] = v / channels;
        op = (op + 1) % nfft;
    }
}

static void input_data_mixed_int24_max(const guint8* _in, gfloat* out, guint len, guint channels, gfloat max_value, guint op, guint nfft) {
    guint i, j;
    gfloat v = 0.0;

    for (j = 0; j < len; j++) {
        for (i = 0; i < channels; i++) {
#if G_BYTE_ORDER == G_BIG_ENDIAN
            gint32 value = GST_READ_UINT24_BE(_in);
#else
            gint32 value = GST_READ_UINT24_LE(_in);
#endif
            if (value & 0x00800000)
                value |= 0xff000000;
            v += value / max_value;
            _in += 3;
        }
        out[op] = v / channels;
        op = (op + 1) % nfft;
    }
}

static void input_data_mixed_int16_max(const guint8* _in, gfloat* out, guint len, guint channels, gfloat max_value, guint op, guint nfft) {
    guint i, j, ip = 0;
    gint16* in = (gint16*)_in;
    gfloat v;

    for (j = 0; j < len; j++) {
        v = in[ip++] / max_value;
        for (i = 1; i < channels; i++)
            v += in[ip++] / max_value;
        out[op] = v / channels;
        op = (op + 1) % nfft;
    }
}

/* non mixing data readers */
static void input_data_float(const guint8* _in, gfloat* out, guint len, guint channels, gfloat max_value, guint op, guint nfft) {
    guint j, ip;
    gfloat* in = (gfloat*)_in;

    for (j = 0, ip = 0; j < len; j++, ip += channels) {
        out[op] = in[ip];
        op = (op + 1) % nfft;
    }
}

static void input_data_double(const guint8* _in, gfloat* out, guint len, guint channels, gfloat max_value, guint op, guint nfft) {
    guint j, ip;
    gdouble* in = (gdouble*)_in;

    for (j = 0, ip = 0; j < len; j++, ip += channels) {
        out[op] = in[ip];
        op = (op + 1) % nfft;
    }
}

static void input_data_int32_max(const guint8* _in, gfloat* out, guint len, guint channels, gfloat max_value, guint op, guint nfft) {
    guint j, ip;
    gint32* in = (gint32*)_in;

    for (j = 0, ip = 0; j < len; j++, ip += channels) {
        out[op] = in[ip] / max_value;
        op = (op + 1) % nfft;
    }
}

static void input_data_int24_max(const guint8* _in, gfloat* out, guint len, guint channels, gfloat max_value, guint op, guint nfft) {
    guint j;

    for (j = 0; j < len; j++) {
#if G_BYTE_ORDER == G_BIG_ENDIAN
        gint32 v = GST_READ_UINT24_BE(_in);
#else
        gint32 v = GST_READ_UINT24_LE(_in);
#endif
        if (v & 0x00800000)
            v |= 0xff000000;
        _in += 3 * channels;
        out[op] = v / max_value;
        op = (op + 1) % nfft;
    }
}

static void input_data_int16_max(const guint8* _in, gfloat* out, guint len, guint channels, gfloat max_value, guint op, guint nfft) {
    guint j, ip;
    gint16* in = (gint16*)_in;

    for (j = 0, ip = 0; j < len; j++, ip += channels) {
        out[op] = in[ip] / max_value;
        op = (op + 1) % nfft;
    }
}

static gboolean gst_magresponse_setup(GstAudioFilter* base, const GstAudioInfo* info) {
    GstMagresponse* mr = GST_MAGRESPONSE(base);
    gboolean multi_channel = mr->multi_channel;
    GstMagresponseInputData input_data = NULL;

    g_mutex_lock(&mr->lock);
    switch (GST_AUDIO_INFO_FORMAT(info)) {
    case GST_AUDIO_FORMAT_S16:
        input_data = multi_channel ? input_data_int16_max : input_data_mixed_int16_max;
        break;
    case GST_AUDIO_FORMAT_S24:
        input_data = multi_channel ? input_data_int24_max : input_data_mixed_int24_max;
        break;
    case GST_AUDIO_FORMAT_S32:
        input_data = multi_channel ? input_data_int32_max : input_data_mixed_int32_max;
        break;
    case GST_AUDIO_FORMAT_F32:
        input_data = multi_channel ? input_data_float : input_data_mixed_float;
        break;
    case GST_AUDIO_FORMAT_F64:
        input_data = multi_channel ? input_data_double : input_data_mixed_double;
        break;
    default:
        g_assert_not_reached();
        break;
    }
    mr->input_data = input_data;

    gst_magresponse_reset_state(mr);
    g_mutex_unlock(&mr->lock);

    return TRUE;
}

static GValue* gst_magresponse_message_add_container(GstStructure* s, GType type, const gchar* name) {
    GValue v = { 0, };
    g_value_init(&v, type);
    /* will copy-by-value */
    gst_structure_set_value(s, name, &v);
    g_value_unset(&v);
    return (GValue*)gst_structure_get_value(s, name);
}

static void gst_magresponse_message_add_list(GValue* cv, gfloat* data, guint num_values) {
    GValue v = { 0, };
    guint i;
    g_value_init(&v, G_TYPE_FLOAT);
    for (i = 0; i < num_values; i++) {
        g_value_set_float(&v, data[i]);
        gst_value_list_append_value(cv, &v); /* copies by value */
    }
    g_value_unset(&v);
}

static void gst_magresponse_message_add_array(GValue* cv, gfloat* data, guint num_values) {
    GValue v = { 0,};
    GValue a = { 0, };
    guint i;
    g_value_init(&a, GST_TYPE_ARRAY);
    g_value_init(&v, G_TYPE_FLOAT);
    for (i = 0; i < num_values; i++) {
        g_value_set_float(&v, data[i]);
        gst_value_array_append_value(&a, &v); /* copies by value */
    }
    g_value_unset(&v);
    gst_value_array_append_value(cv, &a); /* copies by value */
    g_value_unset(&a);
}
