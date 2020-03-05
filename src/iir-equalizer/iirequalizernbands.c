#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "iirequalizer.h"
#include "iirequalizernbands.h"

enum { PROP_NUM_BANDS = 1 };

static void iir_equalizer_nbands_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec);
static void iir_equalizer_nbands_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec);

GST_DEBUG_CATEGORY_EXTERN(equalizer_debug);
#define GST_CAT_DEFAULT equalizer_debug

#define iir_equalizer_nbands_parent_class parent_class
G_DEFINE_TYPE(IirEqualizerNBands, iir_equalizer_nbands, TYPE_IIR_EQUALIZER);

/* equalizer implementation */

static void iir_equalizer_nbands_class_init(IirEqualizerNBandsClass* klass) {
    GObjectClass* gobject_class = (GObjectClass*)klass;
    GstElementClass* gstelement_class = (GstElementClass*)klass;

    gobject_class->set_property = iir_equalizer_nbands_set_property;
    gobject_class->get_property = iir_equalizer_nbands_get_property;

    g_object_class_install_property(
        gobject_class, PROP_NUM_BANDS,
        g_param_spec_uint("num-bands", "num-bands", "number of different bands to use", 1, 64, 10, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));

    gst_element_class_set_static_metadata(gstelement_class, "N Band Equalizer", "Filter/Effect/Audio", "Direct Form IIR equalizer",
                                          "Benjamin Otte <otte@gnome.org>,"
                                          " Stefan Kost <ensonic@users.sf.net>,"
                                          " Tyler Snedigar <snedigart@gmail.com");
}

static void iir_equalizer_nbands_init(IirEqualizerNBands* equ_n) {
    IirEqualizer* equ = IIR_EQUALIZER(equ_n);

    iir_equalizer_compute_frequencies(equ, 10);
}

static void iir_equalizer_nbands_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec) {
    IirEqualizer* equ = IIR_EQUALIZER(object);

    switch (prop_id) {
    case PROP_NUM_BANDS:
        iir_equalizer_compute_frequencies(equ, g_value_get_uint(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void iir_equalizer_nbands_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec) {
    IirEqualizer* equ = IIR_EQUALIZER(object);

    switch (prop_id) {
    case PROP_NUM_BANDS:
        g_value_set_uint(value, equ->freq_band_count);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}
