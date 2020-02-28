#ifndef __GST_MAGRESPONSE_H__
#define __GST_MAGRESPONSE_H__

#include <glib.h>
#include <gst/gst.h>
#include <gst/audio/gstaudiofilter.h>
#include <gst/fft/gstfftf32.h>

G_BEGIN_DECLS

#define GST_TYPE_MAGRESPONSE (gst_magresponse_get_type())
#define GST_MAGRESPONSE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_MAGRESPONSE,GstMagresponse))
#define GST_IS_MAGRESPONSE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_MAGRESPONSE))
#define GST_MAGRESPONSE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_MAGRESPONSE,GstMagresponseClass))
#define GST_IS_MAGRESPONSE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_MAGRESPONSE))
typedef struct _GstMagresponse GstMagresponse;
typedef struct _GstMagresponseClass GstMagresponseClass;
typedef struct _GstMagresponseChannel GstMagresponseChannel;

typedef void (*GstMagresponseInputData)(const guint8* in, gfloat* out, guint len, guint channels, gfloat max_value, guint op, guint nfft);

struct _GstMagresponseChannel {
    gfloat* input;
    gfloat* input_tmp;
    gfloat* magnitude; // accumulated magnitude
    gfloat* phase;     // accumulated phase
    GstFFTF32* fft_ctx;
    GstFFTF32Complex* freqdata;
};

struct _GstMagresponse {
    GstAudioFilter parent;

    guint64 interval; // nanos between emits
    guint64 frames_per_interval;
    guint64 frames_todo;
    guint bands;
    // gint threshold;
    gboolean multi_channel;
    guint64 num_frames;
    guint64 num_fft;
    GstClockTime message_ts;

    // private
    GstMagresponseChannel* channel_data;
    guint num_channels;
    guint input_pos;
    guint64 error_per_interval;
    guint64 accumulated_error;

    GMutex lock;

    GstMagresponseInputData input_data;
};

struct _GstMagresponseClass {
    GstAudioFilterClass parent_class;
};

GType gst_magresponse_get_type(void);

G_END_DECLS

#endif // __GST_FRERESPONSE_H__
