/* GStreamer
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

#ifndef __IIR_EQUALIZER_NBANDS__
#define __IIR_EQUALIZER_NBANDS__

#include "iirequalizer.h"

typedef struct _IirEqualizerNBands IirEqualizerNBands;
typedef struct _IirEqualizerNBandsClass IirEqualizerNBandsClass;

#define TYPE_IIR_EQUALIZER_NBANDS (iir_equalizer_nbands_get_type())
#define IIR_EQUALIZER_NBANDS(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_IIR_EQUALIZER_NBANDS, IirEqualizerNBands))
#define IIR_EQUALIZER_NBANDS_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), TYPE_IIR_EQUALIZER_NBANDS, IirEqualizerNBandsClass))
#define IS_IIR_EQUALIZER_NBANDS(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_IIR_EQUALIZER_NBANDS))
#define IS_IIR_EQUALIZER_NBANDS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), TYPE_IIR_EQUALIZER_NBANDS))

struct _IirEqualizerNBands {
    IirEqualizer equalizer;
};

struct _IirEqualizerNBandsClass {
    IirEqualizerClass equalizer_class;
};

extern GType iir_equalizer_nbands_get_type(void);

#endif /* __IIR_EQUALIZER_NBANDS__ */
