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

#include "aztec_rs.h"

#include <string.h>

/* Galois field */
typedef struct aztec_gf {
    gint ref_count;
    guint logmod;
    guint* logt;
    guint* alog;
} AztecGF;

/* Reed-Solomon encoder */
typedef struct aztec_rs {
    AztecGF* gf;
    guint* poly;
    guint size;
} AztecRS;

static
AztecGF*
aztec_gf_new(
    guint poly)
{
    AztecGF* gf = g_slice_new(AztecGF);
    guint32 size = 0x80000000;
    guint p, v;

    g_atomic_int_set(&gf->ref_count, 1);

    while ((size - 1) > poly) {
        size >>= 1;
    }

    gf->logmod = size - 1;
    gf->logt = g_new(guint, size);
    gf->alog = g_new(guint, size - 1);
    gf->logt[0] = 0;
    for (p = 1, v = 0; v < gf->logmod; v++) {
        gf->alog[v] = p;
        gf->logt[p] = v;
        p <<= 1;
        if (p >= size) {
            p ^= poly;
        }
    }
    return gf;
}

static
AztecGF*
aztec_gf_ref(
    AztecGF* gf)
{
    g_atomic_int_inc(&gf->ref_count);
    return gf;
}

static
void
aztec_gf_unref(
    AztecGF* gf)
{
    if (g_atomic_int_dec_and_test(&gf->ref_count)) {
        g_free(gf->logt);
        g_free(gf->alog);
        g_slice_free1(sizeof(*gf), gf);
    }
}

static
AztecRS*
aztec_rs_new(
    AztecGF* gf,
    guint size,
    guint index)
{
    AztecRS* rs = g_slice_new0(AztecRS);
    guint* poly = g_new(guint, size + 1);
    const guint logmod = gf->logmod;
    const guint* alog = gf->alog;
    const guint* logt = gf->logt;
    guint m, k;

    rs->gf = aztec_gf_ref(gf);
    rs->poly = poly;
    rs->size = size;

    poly[0] = 1;
    for (m = 1; m <= size; m++) {
        poly[m] = 1;
        for (k = m - 1; k > 0; k--) {
            if (poly[k]) {
                poly[k] = alog[(logt[poly[k]] + index) % logmod];
            }
            poly[k] ^= poly[k - 1];
        }
        poly[0] = alog[(logt[poly[0]] + index) % logmod];
        index++;
    }
    return rs;
}

static
AztecRS*
aztec_rs_new_full(
    guint gfpoly,
    guint nsym,
    guint index)
{
    AztecGF* gf = aztec_gf_new(gfpoly);
    AztecRS* rs = aztec_rs_new(gf, nsym, index);

    aztec_gf_unref(gf);
    return rs;
}

static
void
aztec_rs_free(
    AztecRS* rs)
{
    aztec_gf_unref(rs->gf);
    g_free(rs->poly);
    g_slice_free1(sizeof(*rs), rs);
}

static
void
aztec_rs_encode16(
    AztecRS* rs,
    const guint16* data,
    guint len,
    guint16* ecc)
{
    guint i, k;
    AztecGF* gf = rs->gf;
    const guint logmod = gf->logmod;
    const guint* alog = gf->alog;
    const guint* logt = gf->logt;
    const guint* poly = rs->poly;
    const guint last = rs->size - 1;
    const int p0 = poly[0];

    /*
     * Note that the output order is kind of inverted (the most significant
     * coefficient goes first).
     */
    memset(ecc, 0, rs->size * sizeof(*ecc));
    for (i = 0; i < len; i++) {
        const guint m = ecc[0] ^ data[i];

        for (k = rs->size - 1; k > 0; k--) {
            const guint j = rs->size - k - 1;

            ecc[j] = ecc[j + 1];
            if (m && poly[k]) {
                ecc[j] ^= alog[(logt[m] + logt[poly[k]]) % logmod];
            }
        }
        ecc[last] = (m && p0) ? alog[(logt[m] + logt[p0]) % logmod] :  0;
    }
}

void
aztec_rs_encode16_full(
    guint gfpoly,
    guint index,
    const guint16* data,
    guint data_count,
    guint16* ecc,
    guint ecc_count)
{
    AztecRS* rs = aztec_rs_new_full(gfpoly, ecc_count, index);

    aztec_rs_encode16(rs, data, data_count, ecc);
    aztec_rs_free(rs);
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
