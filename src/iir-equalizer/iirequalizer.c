/* GStreamer
 * Copyright (C) <2004> Benjamin Otte <otte@gnome.org>
 *               <2007> Stefan Kost <ensonic@users.sf.net>
 *               <2007> Sebastian Dröge <slomo@circular-chaos.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "iirequalizer.h"
#include "iirequalizernbands.h"

GST_DEBUG_CATEGORY(equalizer_debug);
#define GST_CAT_DEFAULT equalizer_debug

#define BANDS_LOCK(equ) g_mutex_lock(&equ->bands_lock)
#define BANDS_UNLOCK(equ) g_mutex_unlock(&equ->bands_lock)

static void iir_equalizer_child_proxy_interface_init(gpointer g_iface, gpointer iface_data);

static void iir_equalizer_finalize(GObject* object);

static gboolean iir_equalizer_setup(GstAudioFilter* filter, const GstAudioInfo* info);
static GstFlowReturn iir_equalizer_transform_ip(GstBaseTransform* btrans, GstBuffer* buf);
static void set_passthrough(IirEqualizer* equ);

#define ALLOWED_CAPS                              \
    "audio/x-raw,"                                \
    " format=(string) {" GST_AUDIO_NE(F32) " }, " \
    " rate=(int)[1000,MAX],"                      \
    " channels=(int)[1,MAX],"                     \
    " layout=(string)interleaved"

#define iir_equalizer_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE(IirEqualizer, iir_equalizer, GST_TYPE_AUDIO_FILTER,
                        G_IMPLEMENT_INTERFACE(GST_TYPE_CHILD_PROXY, iir_equalizer_child_proxy_interface_init) G_IMPLEMENT_INTERFACE(GST_TYPE_PRESET, NULL));

/* child object */

enum { PROP_GAIN = 1, PROP_FREQ, PROP_BANDWIDTH, PROP_TYPE };

typedef enum { BAND_TYPE_PEAK = 0, BAND_TYPE_LOW_SHELF, BAND_TYPE_HIGH_SHELF } IirEqualizerBandType;

#define TYPE_IIR_EQUALIZER_BAND_TYPE (iir_equalizer_band_type_get_type())
static GType iir_equalizer_band_type_get_type(void) {
    static GType gtype = 0;

    if (gtype == 0) {
        static const GEnumValue values[] = {
            {BAND_TYPE_PEAK, "Peak filter (default for inner bands)", "peak"},
            {BAND_TYPE_LOW_SHELF, "Low shelf filter (default for first band)", "low-shelf"},
            {BAND_TYPE_HIGH_SHELF, "High shelf filter (default for last band)", "high-shelf"},
            {0, NULL, NULL}
        };

        gtype = g_enum_register_static("IirEqualizerBandType", values);
    }
    return gtype;
}

typedef struct _IirEqualizerBandClass IirEqualizerBandClass;

#define TYPE_IIR_EQUALIZER_BAND (iir_equalizer_band_get_type())
#define IIR_EQUALIZER_BAND(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_IIR_EQUALIZER_BAND, IirEqualizerBand))
#define IIR_EQUALIZER_BAND_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), TYPE_IIR_EQUALIZER_BAND, IirEqualizerBandClass))
#define IS_IIR_EQUALIZER_BAND(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_IIR_EQUALIZER_BAND))
#define IS_IIR_EQUALIZER_BAND_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), TYPE_IIR_EQUALIZER_BAND))

struct _IirEqualizerBand {
    GstObject object;

    /*< private > */
    /* center frequency and gain */
    gdouble freq;
    gdouble gain;
    gdouble width;
    IirEqualizerBandType type;

    /* second order iir filter */
    gdouble b0, b1, b2;
    gdouble a1, a2;
};

struct _IirEqualizerBandClass {
    GstObjectClass parent_class;
};

static GType iir_equalizer_band_get_type(void);

static void iir_equalizer_band_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec) {
    IirEqualizerBand* band = IIR_EQUALIZER_BAND(object);
    IirEqualizer* equ = IIR_EQUALIZER(gst_object_get_parent(GST_OBJECT(band)));

    switch (prop_id) {
    case PROP_GAIN: {
        gdouble gain;

        gain = g_value_get_double(value);
        GST_DEBUG_OBJECT(band, "gain = %lf -> %lf", band->gain, gain);
        if (gain != band->gain) {
            BANDS_LOCK(equ);
            equ->need_new_coefficients = TRUE;
            band->gain = gain;
            set_passthrough(equ);
            BANDS_UNLOCK(equ);
            GST_DEBUG_OBJECT(band, "changed gain = %lf ", band->gain);
        }
        break;
    }
    case PROP_FREQ: {
        gdouble freq;

        freq = g_value_get_double(value);
        GST_DEBUG_OBJECT(band, "freq = %lf -> %lf", band->freq, freq);
        if (freq != band->freq) {
            BANDS_LOCK(equ);
            equ->need_new_coefficients = TRUE;
            band->freq = freq;
            BANDS_UNLOCK(equ);
            GST_DEBUG_OBJECT(band, "changed freq = %lf ", band->freq);
        }
        break;
    }
    case PROP_BANDWIDTH: {
        gdouble width;

        width = g_value_get_double(value);
        GST_DEBUG_OBJECT(band, "width = %lf -> %lf", band->width, width);
        if (width != band->width) {
            BANDS_LOCK(equ);
            equ->need_new_coefficients = TRUE;
            band->width = width;
            BANDS_UNLOCK(equ);
            GST_DEBUG_OBJECT(band, "changed width = %lf ", band->width);
        }
        break;
    }
    case PROP_TYPE: {
        IirEqualizerBandType type;

        type = g_value_get_enum(value);
        GST_DEBUG_OBJECT(band, "type = %d -> %d", band->type, type);
        if (type != band->type) {
            BANDS_LOCK(equ);
            equ->need_new_coefficients = TRUE;
            band->type = type;
            BANDS_UNLOCK(equ);
            GST_DEBUG_OBJECT(band, "changed type = %d ", band->type);
        }
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }

    gst_object_unref(equ);
}

static void iir_equalizer_band_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec) {
    IirEqualizerBand* band = IIR_EQUALIZER_BAND(object);

    switch (prop_id) {
    case PROP_GAIN:
        g_value_set_double(value, band->gain);
        break;
    case PROP_FREQ:
        g_value_set_double(value, band->freq);
        break;
    case PROP_BANDWIDTH:
        g_value_set_double(value, band->width);
        break;
    case PROP_TYPE:
        g_value_set_enum(value, band->type);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void iir_equalizer_band_class_init(IirEqualizerBandClass* klass) {
    GObjectClass* gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->set_property = iir_equalizer_band_set_property;
    gobject_class->get_property = iir_equalizer_band_get_property;

    g_object_class_install_property(gobject_class, PROP_GAIN, g_param_spec_double("gain", "gain", "gain for the frequency band ranging from -24.0 dB to +24.0 dB", -24.0, 24.0, 0.0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_CONTROLLABLE));

    g_object_class_install_property(
        gobject_class, PROP_FREQ,
        g_param_spec_double("freq", "freq", "center frequency of the band", 0.0, 48000.0, 0.0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_CONTROLLABLE));

    g_object_class_install_property(
        gobject_class, PROP_BANDWIDTH,
        g_param_spec_double("bandwidth", "bandwidth", "difference between bandedges in Hz", 0.0, 48000.0, 1.0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_CONTROLLABLE));

    g_object_class_install_property(
        gobject_class, PROP_TYPE,
        g_param_spec_enum("type", "Type", "Filter type", TYPE_IIR_EQUALIZER_BAND_TYPE, BAND_TYPE_PEAK, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_CONTROLLABLE));
}

static void iir_equalizer_band_init(IirEqualizerBand* band, IirEqualizerBandClass* klass) {
    band->freq = 0.0;
    band->gain = 0.0;
    band->width = 1.0;
    band->type = BAND_TYPE_PEAK;
}

static GType iir_equalizer_band_get_type(void) {
    static GType type = 0;

    if (G_UNLIKELY(!type)) {
        const GTypeInfo type_info = {
            sizeof(IirEqualizerBandClass),
            NULL,
            NULL,
            (GClassInitFunc)iir_equalizer_band_class_init,
            NULL,
            NULL,
            sizeof(IirEqualizerBand),
            0,
            (GInstanceInitFunc)iir_equalizer_band_init,
        };
        type = g_type_register_static(GST_TYPE_OBJECT, "IirEqualizerBand", &type_info, 0);
    }
    return (type);
}

/* child proxy iface */
static GObject* iir_equalizer_child_proxy_get_child_by_index(GstChildProxy* child_proxy, guint index) {
    IirEqualizer* equ = IIR_EQUALIZER(child_proxy);
    GObject* ret;

    BANDS_LOCK(equ);
    if (G_UNLIKELY(index >= equ->freq_band_count)) {
        BANDS_UNLOCK(equ);
        g_return_val_if_fail(index < equ->freq_band_count, NULL);
    }

    ret = g_object_ref(G_OBJECT(equ->bands[index]));
    BANDS_UNLOCK(equ);

    GST_LOG_OBJECT(equ, "return child[%d] %" GST_PTR_FORMAT, index, ret);
    return ret;
}

static guint iir_equalizer_child_proxy_get_children_count(GstChildProxy* child_proxy) {
    IirEqualizer* equ = IIR_EQUALIZER(child_proxy);

    GST_LOG("we have %d children", equ->freq_band_count);
    return equ->freq_band_count;
}

static void iir_equalizer_child_proxy_interface_init(gpointer g_iface, gpointer iface_data) {
    GstChildProxyInterface* iface = g_iface;

    GST_DEBUG("initializing iface");

    iface->get_child_by_index = iir_equalizer_child_proxy_get_child_by_index;
    iface->get_children_count = iir_equalizer_child_proxy_get_children_count;
}

/* equalizer implementation */

static void iir_equalizer_class_init(IirEqualizerClass* klass) {
    GstAudioFilterClass* audio_filter_class = (GstAudioFilterClass*)klass;
    GstBaseTransformClass* btrans_class = (GstBaseTransformClass*)klass;
    GObjectClass* gobject_class = (GObjectClass*)klass;
    GstCaps* caps;

    gobject_class->finalize = iir_equalizer_finalize;
    audio_filter_class->setup = iir_equalizer_setup;
    btrans_class->transform_ip = iir_equalizer_transform_ip;
    btrans_class->transform_ip_on_passthrough = FALSE;

    caps = gst_caps_from_string(ALLOWED_CAPS);
    gst_audio_filter_class_add_pad_templates(audio_filter_class, caps);
    gst_caps_unref(caps);
}

static void iir_equalizer_init(IirEqualizer* eq) {
    g_mutex_init(&eq->bands_lock);
    /* Band gains are 0 by default, passthrough until they are changed */
    gst_base_transform_set_passthrough(GST_BASE_TRANSFORM(eq), TRUE);
}

static void iir_equalizer_finalize(GObject* object) {
    IirEqualizer* equ = IIR_EQUALIZER(object);
    gint i;

    for (i = 0; i < equ->freq_band_count; i++) {
        if (equ->bands[i])
            gst_object_unparent(GST_OBJECT(equ->bands[i]));
        equ->bands[i] = NULL;
    }
    equ->freq_band_count = 0;

    g_free(equ->bands);
    g_free(equ->history);

    g_mutex_clear(&equ->bands_lock);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

/* Filter taken from
 *
 * The Equivalence of Various Methods of Computing
 * Biquad Coefficients for Audio Parametric Equalizers
 *
 * by Robert Bristow-Johnson
 *
 * http://www.aes.org/e-lib/browse.cfm?elib=6326
 * http://www.musicdsp.org/files/EQ-Coefficients.pdf
 * http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt
 *
 * The bandwidth method that we use here is the preferred
 * one from this article transformed from octaves to frequency
 * in Hz.
 */
static inline gdouble arg_to_scale(gdouble arg) { return (pow(10.0, arg / 40.0)); }

static gdouble calculate_omega(gdouble freq, gint rate) {
    gdouble omega;

    if (freq / rate >= 0.5)
        omega = G_PI;
    else if (freq <= 0.0)
        omega = 0.0;
    else
        omega = 2.0 * G_PI * (freq / rate);

    return omega;
}

static gdouble calculate_bw(IirEqualizerBand* band, gint rate) {
    gdouble bw = 0.0;

    if (band->width / rate >= 0.5) {
        /* If bandwidth == 0.5 the calculation below fails as tan(G_PI/2)
         * is undefined. So set the bandwidth to a slightly smaller value.
         */
        bw = G_PI - 0.00000001;
    } else if (band->width <= 0.0) {
        /* If bandwidth == 0 this band won't change anything so set
         * the coefficients accordingly. The coefficient calculation
         * below would create coefficients that for some reason amplify
         * the band.
         */
        band->b0 = 1.0;
        band->b1 = 0.0;
        band->b2 = 0.0;
        band->a1 = 0.0;
        band->a2 = 0.0;
    } else {
        bw = 2.0 * G_PI * (band->width / rate);
    }
    return bw;
}

static void setup_peak_filter(IirEqualizer* equ, IirEqualizerBand* band) {
    gint rate = GST_AUDIO_FILTER_RATE(equ);

    g_return_if_fail(rate);

    {
        gdouble gain, omega, bw;
        gdouble alpha = 0, alpha1, alpha2, a0;

        gain = arg_to_scale(band->gain);
        omega = calculate_omega(band->freq, rate);
        bw = calculate_bw(band, rate);
        if (bw == 0.0)
            goto out;

        alpha = tan(bw / 2.0);

        alpha1 = alpha * gain;
        alpha2 = alpha / gain;

        a0 = (1.0 + alpha2);

        band->b0 = (1 + alpha1) / a0;
        band->b1 = (-2 * cos(omega)) / a0;
        band->b2 = (1 - alpha1) / a0;
        band->a1 = (-2 * cos(omega)) / a0;
        band->a2 = (1.0 - alpha2) / a0;

    out:
        GST_INFO("rate = %d, gain = %5.1f, width= %7.2f, freq = %7.2f, alpha = %7.5f, b0 = %7.5g, b1 = %7.5g, b2 = %7.5g, a1 = %7.5g, a2 = %7.5g",
            rate, band->gain, band->width, band->freq, alpha, band->b0, band->b1, band->b2, band->a1, band->a2);
    }
}

static void setup_low_shelf_filter(IirEqualizer* equ, IirEqualizerBand* band) {
    gint rate = GST_AUDIO_FILTER_RATE(equ);

    g_return_if_fail(rate);

    {
        gdouble gain, omega, bw;
        gdouble alpha, delta, a0;
        gdouble egp, egm;

        gain = arg_to_scale(band->gain);
        omega = calculate_omega(band->freq, rate);
        bw = calculate_bw(band, rate);
        if (bw == 0.0)
            goto out;

        egm = gain - 1.0;
        egp = gain + 1.0;
        alpha = tan(bw / 2.0);

        delta = 2.0 * sqrt(gain) * alpha;
        a0 = egp + egm * cos(omega) + delta;

        band->b0 = ((egp - egm * cos(omega) + delta) * gain) / a0;
        band->b1 = ((egm - egp * cos(omega)) * 2.0 * gain) / a0;
        band->b2 = ((egp - egm * cos(omega) - delta) * gain) / a0;
        band->a1 = -((egm + egp * cos(omega)) * 2.0) / a0;
        band->a2 = ((egp + egm * cos(omega) - delta)) / a0;

    out:
        GST_INFO("gain = %5.1f, width= %7.2f, freq = %7.2f, b0 = %7.5g, b1 = %7.5g, b2=%7.5g a1 = %7.5g, a2 = %7.5g",
            band->gain, band->width, band->freq, band->b0, band->b1, band->b2, band->a1, band->a2);
    }
}

static void setup_high_shelf_filter(IirEqualizer* equ, IirEqualizerBand* band) {
    gint rate = GST_AUDIO_FILTER_RATE(equ);

    g_return_if_fail(rate);

    {
        gdouble gain, omega, bw;
        gdouble alpha, delta, a0;
        gdouble egp, egm;

        gain = arg_to_scale(band->gain);
        omega = calculate_omega(band->freq, rate);
        bw = calculate_bw(band, rate);
        if (bw == 0.0)
            goto out;

        egm = gain - 1.0;
        egp = gain + 1.0;
        alpha = tan(bw / 2.0);

        delta = 2.0 * sqrt(gain) * alpha;
        a0 = egp - egm * cos(omega) + delta;

        band->b0 = ((egp + egm * cos(omega) + delta) * gain) / a0;
        band->b1 = ((egm + egp * cos(omega)) * -2.0 * gain) / a0;
        band->b2 = ((egp + egm * cos(omega) - delta) * gain) / a0;
        band->a1 = -((egm - egp * cos(omega)) * -2.0) / a0;
        band->a2 = ((egp - egm * cos(omega) - delta)) / a0;

    out:
        GST_INFO("gain = %5.1f, width= %7.2f, freq = %7.2f, b0 = %7.5g, b1 = %7.5g, b2=%7.5g a1 = %7.5g, a2 = %7.5g",
            band->gain, band->width, band->freq, band->b0, band->b1, band->b2, band->a1, band->a2);
    }
}

/* Must be called with bands_lock and transform lock! */
static void set_passthrough(IirEqualizer* equ) {
    gint i;
    gboolean passthrough = TRUE;

    for (i = 0; i < equ->freq_band_count; i++) {
        passthrough = passthrough && (equ->bands[i]->gain == 0.0);
    }

    gst_base_transform_set_passthrough(GST_BASE_TRANSFORM(equ), passthrough);
    GST_DEBUG("Passthrough mode: %d\n", passthrough);
}

static void post_coefficient_change_message(IirEqualizer* eq, IirEqualizerBand* band) {
    GstMessage* msg;
    GstStructure* s;
    guint rate;
    GValue name = G_VALUE_INIT;

    g_value_init(&name, G_TYPE_STRING);
    g_object_get_property(G_OBJECT(band), "name", &name);

    rate = GST_AUDIO_FILTER_RATE(eq);

    s = gst_structure_new("band-info",
        "band", G_TYPE_STRING, g_value_get_string(&name),
        "rate", G_TYPE_UINT, rate,
        "freq", G_TYPE_DOUBLE, band->freq,
        "type", G_TYPE_INT, band->type,
        "gain", G_TYPE_DOUBLE, band->gain,
        "q", G_TYPE_DOUBLE, band->width,
        "b0", G_TYPE_DOUBLE, band->b0,
        "b1", G_TYPE_DOUBLE, band->b1,
        "b2", G_TYPE_DOUBLE, band->b2,
        "a1", G_TYPE_DOUBLE, band->a1,
        "a2", G_TYPE_DOUBLE, band->a2,
        NULL);
    msg = gst_message_new_element(GST_OBJECT(eq), s); // takes ownership of s
    gst_element_post_message(GST_ELEMENT(eq), msg); // takes ownership of msg
}

/* Must be called with bands_lock and transform lock! */
static void update_coefficients(IirEqualizer* equ) {
    gint i, n = equ->freq_band_count;

    for (i = 0; i < n; i++) {
        if (equ->bands[i]->type == BAND_TYPE_PEAK)
            setup_peak_filter(equ, equ->bands[i]);
        else if (equ->bands[i]->type == BAND_TYPE_LOW_SHELF)
            setup_low_shelf_filter(equ, equ->bands[i]);
        else
            setup_high_shelf_filter(equ, equ->bands[i]);

        post_coefficient_change_message(equ, equ->bands[i]);
    }

    equ->need_new_coefficients = FALSE;
}

/* Must be called with transform lock! */
static void alloc_history(IirEqualizer* equ, const GstAudioInfo* info) {
    /* free + alloc = no memcpy */
    g_free(equ->history);
    equ->history = g_malloc0(equ->history_size * GST_AUDIO_INFO_CHANNELS(info) * equ->freq_band_count);
}

void iir_equalizer_compute_frequencies(IirEqualizer* equ, guint new_count) {
    guint old_count, i;
    gdouble freq0, freq1, step;
    gchar name[20];

    if (equ->freq_band_count == new_count)
        return;

    BANDS_LOCK(equ);

    if (G_UNLIKELY(equ->freq_band_count == new_count)) {
        BANDS_UNLOCK(equ);
        return;
    }

    old_count = equ->freq_band_count;
    equ->freq_band_count = new_count;
    GST_DEBUG("bands %u -> %u", old_count, new_count);

    if (old_count < new_count) {
        /* add new bands */
        equ->bands = g_realloc(equ->bands, sizeof(GstObject*) * new_count);
        for (i = old_count; i < new_count; i++) {
            /* otherwise they get names like 'iirequalizerband5' */
            sprintf(name, "band%u", i);
            equ->bands[i] = g_object_new(TYPE_IIR_EQUALIZER_BAND, "name", name, NULL);
            GST_DEBUG("adding band[%d]=%p", i, equ->bands[i]);

            gst_object_set_parent(GST_OBJECT(equ->bands[i]), GST_OBJECT(equ));
            gst_child_proxy_child_added(GST_CHILD_PROXY(equ), G_OBJECT(equ->bands[i]), name);
        }
    } else {
        /* free unused bands */
        for (i = new_count; i < old_count; i++) {
            GST_DEBUG("removing band[%d]=%p", i, equ->bands[i]);
            gst_child_proxy_child_removed(GST_CHILD_PROXY(equ), G_OBJECT(equ->bands[i]), GST_OBJECT_NAME(equ->bands[i]));
            gst_object_unparent(GST_OBJECT(equ->bands[i]));
            equ->bands[i] = NULL;
        }
    }

    alloc_history(equ, GST_AUDIO_FILTER_INFO(equ));

    /* set center frequencies and name band objects
     * FIXME: arg! we can't change the name of parented objects :(
     *   application should read band->freq to get the name
     */

    step = pow(HIGHEST_FREQ / LOWEST_FREQ, 1.0 / new_count);
    freq0 = LOWEST_FREQ;
    for (i = 0; i < new_count; i++) {
        freq1 = freq0 * step;

        if (i == 0)
            equ->bands[i]->type = BAND_TYPE_LOW_SHELF;
        else if (i == new_count - 1)
            equ->bands[i]->type = BAND_TYPE_HIGH_SHELF;
        else
            equ->bands[i]->type = BAND_TYPE_PEAK;

        equ->bands[i]->freq = freq0 + ((freq1 - freq0) / 2.0);
        equ->bands[i]->width = freq1 - freq0;
        GST_DEBUG("band[%2d] = '%lf'", i, equ->bands[i]->freq);

        g_object_notify(G_OBJECT(equ->bands[i]), "bandwidth");
        g_object_notify(G_OBJECT(equ->bands[i]), "freq");
        g_object_notify(G_OBJECT(equ->bands[i]), "type");

        /*
           if(equ->bands[i]->freq<10000.0)
           sprintf (name,"%dHz",(gint)equ->bands[i]->freq);
           else
           sprintf (name,"%dkHz",(gint)(equ->bands[i]->freq/1000.0));
           gst_object_set_name( GST_OBJECT (equ->bands[i]), name);
           GST_DEBUG ("band[%2d] = '%s'",i,name);
         */
        freq0 = freq1;
    }

    equ->need_new_coefficients = TRUE;
    BANDS_UNLOCK(equ);
}

typedef struct {
    gfloat x1, x2; /* history of input values for a filter */
    gfloat y1, y2; /* history of output values for a filter */
} SecondOrderHistory;

static inline gfloat one_step(IirEqualizerBand* filter, SecondOrderHistory* history, gfloat input) {
    /* calculate output */
    gfloat output = filter->b0 * input + filter->b1 * history->x1 + filter->b2 * history->x2 - filter->a1 * history->y1 - filter->a2 * history->y2;
    /* update history */
    history->y2 = history->y1;
    history->y1 = output;
    history->x2 = history->x1;
    history->x1 = input;

    return output;
}

static const guint history_size = sizeof(SecondOrderHistory);

static void iir_equ_process(IirEqualizer* equ, guint8* data, guint size, guint channels) {
    guint frames = size / channels / sizeof(gfloat);
    guint i, c, f, nf = equ->freq_band_count;
    gfloat cur;
    IirEqualizerBand** filters = equ->bands;

    for (i = 0; i < frames; i++) {
        SecondOrderHistory* history = equ->history;
        for (c = 0; c < channels; c++) {
            cur = *((gfloat*)data);
            for (f = 0; f < nf; f++) {
                cur = one_step(filters[f], history, cur);
                history++;
            }
            *((gfloat*)data) = (gfloat)cur;
            data += sizeof(gfloat);
        }
    }
}

// static void iir_equalizer_band_frequency_response(IirEqualizerBand* band, guint rate, guint num_freqs, const float* freqs, float* mag_res) {
//     guint k;
//     // double a0 = band->a0;
//     // double a1 = band->a1;
//     // double a2 = band->a2;
//     // double b1 = band->b1;
//     // double b2 = band->b2;
//     gdouble b0 = band->b0;
//     gdouble b1 = band->b1;
//     gdouble b2 = band->b2;
//     gdouble a1 = band->a1;
//     gdouble a2 = band->a2;

//     for (k = 0; k < num_freqs; ++k) {
//         double omega = calculate_omega(freqs[k], rate);
//         double complex z = CMPLX(cos(omega), sin(omega));
//         double complex numerator = b0 + (b1 + b2 * z) * z;
//         double complex denominator = CMPLX(1, 0) + (a1 + a2 * z) * z;
//         double complex response = numerator / denominator;
//         mag_res[k] = (float)(fabs(response));
//         g_print("  %f \t -> %f\n", freqs[k], mag_res[k]);
//     }


//     // for (k = 0; k < num_freqs; ++k) {
//     //     double omega = -G_PI * freqs[k];
//     //     double z = tan(omega);
//     //     double num = (b1 + b2*z) * z;
//     //     double den = a0 + (a1 + a2*z) * z;
//     //     double response = num / den;
//     //     mag_res[k] = fabs(response);
//     // }

//     // H(z) = (b0 + b1*z^(-1) + b2*z^(-2))/(1 + a1*z^(-1) + a2*z^(-2))
//     // for (k = 0; k < num_freqs; ++k) {
//     //     double omega = calculate_omega(freqs[k], rate);
//     //     double z = 1.0 / omega;
//     //     double num = a0 + (a1 + a2 * z) * z;
//     //     double denom = 1 + (b1 + b2 * z) * z;
//     //     double res = num / denom;
//     //     mag_res[k] = fabsf((float)res);
//     //     g_print(" %f  -->  %f\n", freqs[k], mag_res[k]);
//     // }
// }

// static void iir_equalizer_frequency_response(IirEqualizer* eq, guint num_freqs, float* mag_res) {
//     guint nbands = eq->freq_band_count;
//     float* freqs = g_malloc_n(num_freqs, sizeof(float));
//     double rate = (double) GST_AUDIO_FILTER_RATE(eq);
//     if (rate <= 0) rate = 44100.0;
//     double m = ((double) num_freqs) / log10((rate / 2.0) / LOWEST_FREQ);
//     guint i, j;
//     for (i = 0; i < num_freqs; i++) {
//         freqs[i] = (pow(10.0, ((double)i) / m) * LOWEST_FREQ); // / (rate / 2.0); // may be able to simplify this
//         mag_res[i] = 1.0;
//     }

//     for (i = 0; i < nbands; i++) {
//         IirEqualizerBand* band = eq->bands[i];
//         float* mr = g_malloc_n(num_freqs, sizeof(float));
//         iir_equalizer_band_frequency_response(band, (guint) rate, num_freqs, freqs, mr);
//         for (j = 0; j < num_freqs; j++) {
//             mag_res[j] *= mr[j];
//         }
//         g_free(mr);
//     }
//     g_free(freqs);
// }

static GstFlowReturn iir_equalizer_transform_ip(GstBaseTransform* btrans, GstBuffer* buf) {
    GstAudioFilter* filter = GST_AUDIO_FILTER(btrans);
    IirEqualizer* equ = IIR_EQUALIZER(btrans);
    GstClockTime timestamp;
    GstMapInfo map;
    gint channels = GST_AUDIO_FILTER_CHANNELS(filter);
    gboolean need_new_coefficients;

    if (G_UNLIKELY(channels < 1 || equ->process == NULL))
        return GST_FLOW_NOT_NEGOTIATED;

    BANDS_LOCK(equ);
    need_new_coefficients = equ->need_new_coefficients;
    BANDS_UNLOCK(equ);

    timestamp = GST_BUFFER_TIMESTAMP(buf);
    timestamp = gst_segment_to_stream_time(&btrans->segment, GST_FORMAT_TIME, timestamp);

    if (GST_CLOCK_TIME_IS_VALID(timestamp)) {
        IirEqualizerBand** filters = equ->bands;
        guint f, nf = equ->freq_band_count;

        gst_object_sync_values(GST_OBJECT(equ), timestamp);

        /* sync values for bands too */
        /* FIXME: iterating equ->bands is not thread-safe here */
        for (f = 0; f < nf; f++) {
            gst_object_sync_values(GST_OBJECT(filters[f]), timestamp);
        }
    }

    BANDS_LOCK(equ);
    if (need_new_coefficients) {
        update_coefficients(equ);
    }
    BANDS_UNLOCK(equ);

    gst_buffer_map(buf, &map, GST_MAP_READWRITE);
    equ->process(equ, map.data, map.size, channels);
    gst_buffer_unmap(buf, &map);

    return GST_FLOW_OK;
}

static gboolean iir_equalizer_setup(GstAudioFilter* audio, const GstAudioInfo* info) {
    IirEqualizer* equ = IIR_EQUALIZER(audio);

    switch (GST_AUDIO_INFO_FORMAT(info)) {
    case GST_AUDIO_FORMAT_F32:
        // TODO: these can be set in init now since we only deal with one format
        equ->history_size = history_size;
        equ->process = iir_equ_process;
        break;
    default:
        return FALSE;
    }

    alloc_history(equ, info);
    return TRUE;
}

static gboolean plugin_init(GstPlugin* plugin) {
    GST_DEBUG_CATEGORY_INIT(equalizer_debug, "iirequalizer", 0, "iirequalizer");
    return gst_element_register(plugin, "iirequalizer", GST_RANK_NONE, TYPE_IIR_EQUALIZER_NBANDS);
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, iirequalizer, "eq", plugin_init, "0.1.0", "LGPL", PACKAGE, "https://github.com/pulse0ne/eqnix")
