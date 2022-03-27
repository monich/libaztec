/*
 * Copyright (C) 2019-2022 by Slava Monich <slava@monich.com>
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

#ifndef AZTEC_BITS_H
#define AZTEC_BITS_H

#include <glib.h>

typedef struct aztec_bits {
    guint count;
} AztecBits;

AztecBits*
aztec_bits_new(
    void)
    G_GNUC_INTERNAL;

void
aztec_bits_free(
    AztecBits* bits)
    G_GNUC_INTERNAL;

void
aztec_bits_reserve(
    AztecBits* bits,
    guint count)
    G_GNUC_INTERNAL;

void
aztec_bits_clear(
    AztecBits* bits)
    G_GNUC_INTERNAL;

void
aztec_bits_add(
    AztecBits* bits,
    guint32 value,
    guint nbits)
    G_GNUC_INTERNAL;

void
aztec_bits_add_inv(
    AztecBits* bits,
    guint32 value,
    guint nbits)
    G_GNUC_INTERNAL;

void
aztec_bits_set(
    AztecBits* bits,
    guint offset,
    guint32 value,
    guint nbits)
    G_GNUC_INTERNAL;

guint
aztec_bits_get(
    const AztecBits* bits,
    guint offset,
    guint nbits)
    G_GNUC_INTERNAL;

guint
aztec_bits_get_inv(
    const AztecBits* bits,
    guint offset,
    guint nbits)
    G_GNUC_INTERNAL;

#endif /* AZTEC_BITS_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
