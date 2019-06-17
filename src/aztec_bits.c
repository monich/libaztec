/*
 * Copyright (C) 2019 by Slava Monich <slava@monich.com>
 *
 * You may use this file under the terms of the BSD license as follows:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other materials provided with the
 *      distribution.
 *   3. Neither the names of the copyright holders nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * any official policies, either expressed or implied.
 */

#include "aztec_bits.h"

#include <string.h>

#define UNIT_SHIFT      (5)
#define BITS_PER_UNIT   (1 << UNIT_SHIFT)    /* 32 */
#define BIT_INDEX_MASK  (BITS_PER_UNIT - 1)  /* 0x1f */

#define UNIT_INDEX(bit) (((guint)(bit)) >> UNIT_SHIFT)
#define BIT_INDEX(bit)  ((bit) & BIT_INDEX_MASK)
#define BIT(bit)        (((guint32)1) << BIT_INDEX(bit))

typedef struct aztec_bits_priv {
    AztecBits pub;
    guint32* units;
    guint alloc;
} AztecBitsPriv;

static const guint32 unit_mask[33] = {
    0x00000000, 0x00000001, 0x00000003, 0x00000007,
    0x0000000f, 0x0000001f, 0x0000003f, 0x0000007f,
    0x000000ff, 0x000001ff, 0x000003ff, 0x000007ff,
    0x00000fff, 0x00001fff, 0x00003fff, 0x00007fff,
    0x0000ffff, 0x0001ffff, 0x0003ffff, 0x0007ffff,
    0x000fffff, 0x001fffff, 0x003fffff, 0x007fffff,
    0x00ffffff, 0x01ffffff, 0x03ffffff, 0x07ffffff,
    0x0fffffff, 0x1fffffff, 0x3fffffff, 0x7fffffff,
    0xffffffff
};

static
inline
AztecBitsPriv*
aztec_bits_cast(
    const AztecBits* pub)
{
    return (AztecBitsPriv*)((guint8*)pub - G_STRUCT_OFFSET(AztecBitsPriv,pub));
}

AztecBits*
aztec_bits_new(
    void)
{
    return &(g_slice_new0(AztecBitsPriv))->pub;
}

void
aztec_bits_free(
    AztecBits* bits)
{
    AztecBitsPriv* self = aztec_bits_cast(bits);

    g_free(self->units);
    g_slice_free1(sizeof(*self), self);
}

static
void
aztec_bits_alloc(
    AztecBitsPriv* self,
    guint count)
{
    /* Caller checks that count > 0 */
    const guint n = UNIT_INDEX(count - 1) + 1;

    if (n > self->alloc) {
        const gsize newbytes = (n - self->alloc) * sizeof(guint32);

        self->units = g_realloc(self->units, n * sizeof(guint32));
        memset(self->units + self->alloc, 0, newbytes);
        self->alloc = n;
    }
}

void
aztec_bits_reserve(
    AztecBits* bits,
    guint count)
{
    if (count) {
        aztec_bits_alloc(aztec_bits_cast(bits), count);
    }
}

void
aztec_bits_clear(
    AztecBits* bits)
{
    if (bits->count) {
        AztecBitsPriv* self = aztec_bits_cast(bits);
        const guint n = UNIT_INDEX(bits->count - 1) + 1;

        /* Zero allocated bits */
        memset(self->units, 0,  n * sizeof(guint32));
        bits->count = 0;
    }
}

void
aztec_bits_add(
    AztecBits* bits,
    guint32 value,
    guint nbits)
{
    if (nbits > 0) {
        AztecBitsPriv* self = aztec_bits_cast(bits);

        if (nbits > BITS_PER_UNIT) {
            nbits = BITS_PER_UNIT;
        }
        aztec_bits_alloc(self, bits->count + nbits);
        while (TRUE) {
            guint32* unit;
            const guint next_index = BIT_INDEX(bits->count);
            guint bits_used, bits_avail;

            if (next_index) {
                bits_avail = BITS_PER_UNIT - next_index;
            } else {
                bits_avail = BITS_PER_UNIT;
            }

            bits_used = BITS_PER_UNIT - bits_avail;
            unit = self->units + UNIT_INDEX(bits->count);

            if (nbits > bits_avail) {
                /* Crossing the unit boundary */
                *unit |= (value & unit_mask[bits_avail]) << bits_used;
                bits->count += bits_avail;
                value >>= bits_avail;
                nbits -= bits_avail;
            } else {
                *unit |= (value & unit_mask[nbits]) << bits_used;
                bits->count += nbits;
                break;
            }
        }
    }
}

void
aztec_bits_add_inv(
    AztecBits* bits,
    guint32 value,
    guint nbits)
{
    if (nbits == 1) {
        /* Nothing to invert */
        aztec_bits_add(bits, value, nbits);
    } else if (nbits > 0) {
        if (nbits > BITS_PER_UNIT) {
            nbits = BITS_PER_UNIT;
        }
        if ((value & unit_mask[nbits]) == unit_mask[nbits]) {
            /* Nothing to invert */
            aztec_bits_add(bits, value, nbits);
        } else {
            guint i;
            guint32 inv = 0;

            for (i = 0; i < nbits; i++) {
                inv = (inv << 1) | (value & 1);
                value >>= 1;
            }
            aztec_bits_add(bits, inv, nbits);
        }
    }
}

void
aztec_bits_set(
    AztecBits* bits,
    guint offset,
    guint32 value,
    guint nbits)
{
    AztecBitsPriv* self = aztec_bits_cast(bits);
    guint mincount;

    if (nbits > BITS_PER_UNIT) {
        nbits = BITS_PER_UNIT;
    }

    mincount = offset + nbits;
    if (bits->count < mincount) {
        aztec_bits_alloc(self, mincount);
        bits->count = mincount;
    }

    while (nbits > 0) {
        guint32* unit;
        const guint next_index = BIT_INDEX(offset);
        guint bits_used, bits_avail;

        if (next_index) {
            bits_avail = BITS_PER_UNIT - next_index;
        } else {
            bits_avail = BITS_PER_UNIT;
        }

        bits_used = BITS_PER_UNIT - bits_avail;
        unit = self->units + UNIT_INDEX(offset);

        if (nbits > bits_avail) {
            /* Crossing the unit boundary */
            *unit |= (value & unit_mask[bits_avail]) << bits_used;
            offset += bits_avail;
            value >>= bits_avail;
            nbits -= bits_avail;
        } else {
            *unit |= (value & unit_mask[nbits]) << bits_used;
            break;
        }
    }
}

guint
aztec_bits_get(
    const AztecBits* bits,
    guint offset,
    guint nbits)
{
    guint ret = 0;

    if (nbits > 0 && offset < bits->count) {
        const AztecBitsPriv* self = aztec_bits_cast(bits);
        const guint unit_index = UNIT_INDEX(offset);
        const guint unit_offset = BIT_INDEX(offset);
        const guint32* unit = self->units + unit_index;

        if (offset + nbits > bits->count) {
            nbits = bits->count - offset;
        }
        if (nbits > BITS_PER_UNIT) {
            nbits = BITS_PER_UNIT;
        }
        if (unit_index == UNIT_INDEX(offset + nbits - 1)) {
            /* All bits happen to be in one unit */
            ret = (unit[0] >> unit_offset) & unit_mask[nbits];
        } else {
            const guint nbits0 = BITS_PER_UNIT - unit_offset;

            /* Bits are crossing the unit boundary */
            ret = (unit[0] >> unit_offset) |
                ((unit[1] & unit_mask[nbits - nbits0]) << nbits0);
        }
    }
    return ret;
}

guint
aztec_bits_get_inv(
    const AztecBits* bits,
    guint offset,
    guint nbits)
{
    guint ret = 0;

    if (nbits > BITS_PER_UNIT) {
        nbits = BITS_PER_UNIT;
    }
    ret = aztec_bits_get(bits, offset, nbits);
    if (nbits > 1 && (ret & unit_mask[nbits]) != unit_mask[nbits]) {
        guint32 tmp = ret;
        guint i;

        for (ret = 0, i = 0; i < nbits; i++) {
            ret = (ret << 1) | (tmp & 1);
            tmp >>= 1;
        }
    }
    return ret;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
