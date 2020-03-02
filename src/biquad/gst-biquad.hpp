#ifndef GST_BIQUAD_HPP
#define GST_BIQUAD_HPP

#include <glib.h>
#include <gst/gst.h>
#include <gst/audio/gstaudiofilter.h>
#include "biquad.hpp"

#define GST_TYPE_BIQUAD (GstBiquad::get_type())
#define GST_BIQUAD(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_BIQUAD, GstBiquad))
#define GST_IS_BIQUAD(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_BIQUAD))
#define GST_BIQUAD_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_BIQUAD, GstBiquadClass))
#define GST_IS_BIQUAD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_BIQUAD))
#define GST_BIQUAD_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_BIQUAD, GstBiquadClass))

struct GstBiquad {
public:
    GstAudioFilter parent;
    static GType get_type();

    Biquad* delegate;

    gboolean emit_fr = true;
    guint num_fr_bands = 128;

    gboolean isdoublewide;

private:
    enum {
        PROP_FREQUENCY = 1,
        PROP_Q,
        PROP_GAIN,
        PROP_FILTER_TYPE,
        PROP_FR_NUM_BANDS,
        PROP_EMIT_FR
    };

    static GstAudioFilterClass* s_parent_class;

    // gst plugin stuff
    static void base_init(gpointer g_class);
    static void class_init(gpointer g_class, gpointer class_data);
    static gboolean setup(GstAudioFilter* audio, const GstAudioInfo* info);
    static void init(GTypeInstance* instance, gpointer g_class);
    static void finalize(GObject* object);
    static void set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec);
    static void get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec);
    static GstFlowReturn transform_ip(GstBaseTransform* trans, GstBuffer* in);

    static GstMessage* create_fr_message(GstBiquad* b);
};

struct GstBiquadClass {
    GstAudioFilterClass parent_class;
};

#endif // GST_BIQUAD_HPP