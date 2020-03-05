/* GStreamer IIR equalizer
 * Copyright (C) <2004> Benjamin Otte <otte@gnome.org>
 *               <2007> Stefan Kost <ensonic@users.sf.net>
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

#ifndef __IIR_EQUALIZER__
#define __IIR_EQUALIZER__

#include <gst/audio/gstaudiofilter.h>

typedef struct _IirEqualizer IirEqualizer;
typedef struct _IirEqualizerClass IirEqualizerClass;
typedef struct _IirEqualizerBand IirEqualizerBand;

#define TYPE_IIR_EQUALIZER (iir_equalizer_get_type())
#define IIR_EQUALIZER(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_IIR_EQUALIZER, IirEqualizer))
#define IIR_EQUALIZER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), TYPE_IIR_EQUALIZER, IirEqualizerClass))
#define IS_IIR_EQUALIZER(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_IIR_EQUALIZER))
#define IS_IIR_EQUALIZER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), TYPE_IIR_EQUALIZER))

#define LOWEST_FREQ (10.0)
#define HIGHEST_FREQ (20000.0)

typedef void (*ProcessFunc)(IirEqualizer* eq, guint8* data, guint size, guint channels);

struct _IirEqualizer {
    GstAudioFilter audiofilter;

    /*< private >*/
    GMutex bands_lock;
    IirEqualizerBand** bands;

    /* properties */
    guint freq_band_count;
    /* for each band and channel */
    gpointer history;
    guint history_size;

    gboolean need_new_coefficients;

    ProcessFunc process;
};

struct _IirEqualizerClass {
    GstAudioFilterClass audiofilter_class;
};

extern void iir_equalizer_compute_frequencies(IirEqualizer* equ, guint new_count);

extern GType iir_equalizer_get_type(void);

#endif /* __IIR_EQUALIZER__ */
