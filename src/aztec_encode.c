/*
 * Copyright (C) 2019-2020 by Slava Monich <slava@monich.com>
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

#include "aztec_encode.h"
#include "aztec_bits.h"
#include "aztec_rs.h"

#include <glib.h>
#include <string.h>

#define MODE_BINARY (0x00)
#define MODE_UPPER  (0x01)
#define MODE_LOWER  (0x02)
#define MODE_MIXED  (0x04)
#define MODE_PUNCT  (0x08)
#define MODE_DIGIT  (0x10)

#define LF 10
#define CR 13
#define SP 32

#define MAX_COMPACT_LAYERS (4)
#define MAX_FULL_LAYERS    (32)

typedef
void
(*AztecSymbolFillRowProc)(
    guint8* row,
    guint size,
    AztecBits* bits,
    int i);

typedef struct aztec_pattern {
    guint size;
    const guint16* data;
} AztecPattern;

typedef struct aztec_block AztecBlock;
struct aztec_block {
    AztecBlock* next;
    const guint8* data;
    gsize len;
    guint8 mode;
};

typedef struct aztec_error_correction {
    guint percent;
    guint compact[MAX_COMPACT_LAYERS];
    guint full[MAX_FULL_LAYERS];
} AztecErrorCorrection;

typedef struct aztec_symbol_params {
    guint8 size;
    guint8 cwsize;
    guint16 cwcount;
} AztecSymbolParams;

typedef struct aztec_config {
    gboolean compact;
    guint8 layers;
    guint8 symsize;
    guint8 cwsize;
    guint cwcount;
    guint gfpoly;
    AztecBits* (*encode_mode_message)(guint layers, guint codewords);
    AztecBits* (*encode_symbol)(guint size, AztecBits* data, AztecBits* mode);
} AztecConfig;

typedef struct aztec_builder {
    AztecBits* bits;
    guint8 mode;
    guint8 pop_mode;
    guint binary_offset;
    guint binary_len;
} AztecBuilder;

typedef struct aztec_codewords {
    guint16* words;
    guint count;
} AztecCodewords;

G_STATIC_ASSERT(sizeof(AztecCodewords) == sizeof(GArray));

static
inline
AztecCodewords*
aztec_codewords_sized_new(
    guint reserved)
{
    return (AztecCodewords*)g_array_sized_new(FALSE, FALSE, 2, reserved * 2);
}

static
inline
void
aztec_codewords_append(
    AztecCodewords* codewords,
    guint16 val)
{
    g_array_append_vals((GArray*)codewords, &val, 1);
}

static
inline
void
aztec_codewords_set_count(
    AztecCodewords* codewords,
    guint count)
{
    g_array_set_size((GArray*)codewords, count);
}

static
inline
int*
aztec_codewords_free(
    AztecCodewords* codewords,
    gboolean free_data)
{
    if (codewords) {
        return (int*)g_array_free((GArray*)codewords, free_data);
    } else {
        return NULL;
    }
}

static
gboolean
aztec_bytes_contain(
    GByteArray* sorted,
    guint value)
{
    guint i;

    for (i = 0; i < sorted->len; i++) {
        const guint8 b = sorted->data[i];

        if (b > value) {
            return FALSE;
        } else if (b == value) {
            return TRUE;
        }
    }
    return FALSE;
}

static
inline
void
aztec_encode_builder_add_bits(
    AztecBuilder* builder,
    guint value,
    guint nbits)
{
    aztec_bits_add_inv(builder->bits, value, nbits);
}

static
void
aztec_encode_builder_append_binary_length(
    AztecBuilder* builder,
    guint len)
{
    if (len < 32) {
        /* Last binary block */
        builder->binary_len = len;
        aztec_encode_builder_add_bits(builder, builder->binary_len, 5);
    } else if (len < 63) {
        /*
         * For 32-62 bytes, two 5-bit byte shift sequences are more
         * compact than one 11-bit.
         */
        builder->binary_len = 31;
        aztec_encode_builder_add_bits(builder, builder->binary_len, 5);
    } else {
        const guint maxlen = 0x7ff; /* 11 bits */

        builder->binary_len = (len > maxlen) ? maxlen : len;
        aztec_encode_builder_add_bits(builder, 0, 5);
        aztec_encode_builder_add_bits(builder, builder->binary_len, 11);
    }
}

static
void
aztec_encode_builder_append_binary_data(
    AztecBuilder* builder,
    const guint8* data)
{
    guint i;
    AztecBits* bits = builder->bits;

    aztec_bits_reserve(bits, bits->count + builder->binary_len * 8);
    for (i = 0; i < builder->binary_len; i++) {
        aztec_encode_builder_add_bits(builder, data
            [builder->binary_offset++], 8);
    }
    builder->binary_len = 0;
}

static
void
aztec_encode_builder_append_data(
    AztecBuilder* builder,
    const AztecBlock* block,
    const guint8* map,
    guint nbits)
{
    guint i;
    AztecBits* bits = builder->bits;

    aztec_bits_reserve(bits, bits->count + block->len * nbits);
    for (i = 0; i < block->len; i++) {
        aztec_encode_builder_add_bits(builder, map[block->data[i]], nbits);
    }
}

static
void
aztec_encode_builder_shift_or_latch(
    AztecBuilder* builder,
    const AztecBlock* block)
{
    if (builder->mode != block->mode) {
        switch (builder->mode) {
        case MODE_UPPER:
            switch (block->mode) {
            case MODE_BINARY:
                /* Upper(31) B/S */
                aztec_encode_builder_add_bits(builder, 31, 5);
                aztec_encode_builder_append_binary_length(builder,
                    block->len - builder->binary_offset);
                builder->pop_mode = builder->mode;
                break;
            case MODE_LOWER:
                /* Upper(28) L/L */
                aztec_encode_builder_add_bits(builder, 28, 5);
                break;
            case MODE_MIXED:
                /* Upper(29) M/L */
                aztec_encode_builder_add_bits(builder, 29, 5);
                break;
            case MODE_PUNCT:
                 if (block->len == 1) {
                     /* Upper(0) P/S */
                     aztec_encode_builder_add_bits(builder, 0, 5);
                     builder->pop_mode = builder->mode;
                 } else {
                     /* Upper(29) M/L + Mixed(30) P/L */
                     aztec_encode_builder_add_bits(builder, 29, 5);
                     aztec_encode_builder_add_bits(builder, 30, 5);
                 }
                 break;
            case MODE_DIGIT:
                /* Upper(30) D/L */
                aztec_encode_builder_add_bits(builder, 30, 5);
                break;
            }
            break;
        case MODE_LOWER:
            switch (block->mode) {
            case MODE_BINARY:
                /* Lower(31) B/S */
                aztec_encode_builder_add_bits(builder, 31, 5);
                aztec_encode_builder_append_binary_length(builder,
                    block->len - builder->binary_offset);
                builder->pop_mode = builder->mode;
                break;
            case MODE_UPPER:
                if (block->len == 1) {
                    /* Lower(28) U/S */
                    aztec_encode_builder_add_bits(builder, 28, 5);
                    builder->pop_mode = builder->mode;
                } else {
                    /* Lower(30) D/L + Digit(14) U/L*/
                    aztec_encode_builder_add_bits(builder, 30, 5);
                    aztec_encode_builder_add_bits(builder, 14, 4);
                }
                break;
            case MODE_MIXED:
                /* Lower(29) M/L */
                aztec_encode_builder_add_bits(builder, 29, 5);
                break;
            case MODE_PUNCT:
                 if (block->len == 1) {
                     /* Lower(0) P/S */
                     aztec_encode_builder_add_bits(builder, 0, 5);
                     builder->pop_mode = builder->mode;
                 } else {
                     /* Lower(29) M/L + Mixed(30) P/L */
                     aztec_encode_builder_add_bits(builder, 29, 5);
                     aztec_encode_builder_add_bits(builder, 30, 5);
                 }
                 break;
            case MODE_DIGIT:
                /* Lower(30) D/L */
                aztec_encode_builder_add_bits(builder, 30, 5);
                break;
            }
            break;
        case MODE_MIXED:
            switch (block->mode) {
            case MODE_BINARY:
                /* Mixed(31) B/S */
                aztec_encode_builder_add_bits(builder, 31, 5);
                aztec_encode_builder_append_binary_length(builder,
                    block->len - builder->binary_offset);
                builder->pop_mode = builder->mode;
                break;
            case MODE_UPPER:
                /* Mixed(29) U/L */
                aztec_encode_builder_add_bits(builder, 29, 5);
                break;
            case MODE_LOWER:
                /* Mixed(28) L/L */
                aztec_encode_builder_add_bits(builder, 28, 5);
                break;
            case MODE_PUNCT:
                 if (block->len == 1) {
                     /* Mixed(0) P/S */
                     aztec_encode_builder_add_bits(builder, 0, 5);
                     builder->pop_mode = builder->mode;
                 } else {
                     /* Mixed(30) P/L */
                     aztec_encode_builder_add_bits(builder, 30, 5);
                 }
                 break;
            case MODE_DIGIT:
                /* Mixed(28) L/L + Lower(30) D/L */
                aztec_encode_builder_add_bits(builder, 28, 5);
                aztec_encode_builder_add_bits(builder, 30, 5);
                break;
            }
            break;
        case MODE_PUNCT:
            switch (block->mode) {
            case MODE_BINARY:
                /* Punct(31) U/L + Upper(31) B/S */
                aztec_encode_builder_add_bits(builder, 31, 5);
                aztec_encode_builder_add_bits(builder, 31, 5);
                aztec_encode_builder_append_binary_length(builder,
                    block->len - builder->binary_offset);
                builder->pop_mode = MODE_UPPER;
                break;
            case MODE_UPPER:
                /* Punct(31) U/L */
                aztec_encode_builder_add_bits(builder, 31, 5);
                break;
            case MODE_LOWER:
                /* Punct(31) U/L + Upper(28) L/L */
                aztec_encode_builder_add_bits(builder, 31, 5);
                aztec_encode_builder_add_bits(builder, 28, 5);
                break;
            case MODE_MIXED:
                /* Punct(31) U/L + Upper(29) M/L */
                aztec_encode_builder_add_bits(builder, 31, 5);
                aztec_encode_builder_add_bits(builder, 29, 5);
                break;
            case MODE_DIGIT:
                /* Punct(31) U/L + Upper(30) D/L */
                aztec_encode_builder_add_bits(builder, 31, 5);
                aztec_encode_builder_add_bits(builder, 30, 5);
                break;
            }
            break;
        case MODE_DIGIT:
            switch (block->mode) {
            case MODE_BINARY:
                /* Digit(14) U/L + Upper(31) B/S */
                aztec_encode_builder_add_bits(builder, 14, 4);
                aztec_encode_builder_add_bits(builder, 31, 5);
                aztec_encode_builder_append_binary_length(builder,
                    block->len - builder->binary_offset);
                builder->pop_mode = MODE_UPPER;
                break;
            case MODE_UPPER:
                 if (block->len == 1) {
                     /* Digit(15) U/S */
                     aztec_encode_builder_add_bits(builder, 15, 4);
                     builder->pop_mode = builder->mode;
                 } else {
                     /* Digit(14) U/L */
                     aztec_encode_builder_add_bits(builder, 14, 4);
                 }
                 break;
            case MODE_LOWER:
                /* Digit(14) U/L + Upper(28) L/L */
                aztec_encode_builder_add_bits(builder, 14, 4);
                aztec_encode_builder_add_bits(builder, 28, 5);
                break;
            case MODE_MIXED:
                /* Digit(14) U/L + Upper(29) M/L */
                aztec_encode_builder_add_bits(builder, 14, 4);
                aztec_encode_builder_add_bits(builder, 29, 5);
                break;
            case MODE_PUNCT:
                 if (block->len == 1) {
                     /* Digit(0) P/S */
                     aztec_encode_builder_add_bits(builder, 0, 4);
                     builder->pop_mode = builder->mode;
                 } else {
                     /* Digit(14) U/L + Upper(29) M/L + Mixed(30) P/L*/
                     aztec_encode_builder_add_bits(builder, 14, 4);
                     aztec_encode_builder_add_bits(builder, 29, 5);
                     aztec_encode_builder_add_bits(builder, 30, 5);
                 }
                 break;
            }
            break;
        }
        builder->mode = block->mode;
    }
}

static
AztecBits*
aztec_encode_data_bits(
    const guint8* data,
    gsize len)
{
    /* Prefer Digit encoding because it's 4 bits */
    static const guint8 modesubst[32] = {
        0x00, 0x01, 0x02, 0x01, 0x04, 0x04, 0x04, 0x04,
        0x08, 0x01, 0x02, 0x01, 0x04, 0x04, 0x04, 0x04,
        0x10, 0x01, 0x02, 0x01, 0x04, 0x04, 0x04, 0x04,
        0x08, 0x01, 0x02, 0x01, 0x04, 0x04, 0x04, 0x04
    };

    static const guint8 mode[256] = {
        0x00, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
        0x04, 0x04, 0x0c, 0x04, 0x04, 0x0c, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x04, 0x04, 0x04, 0x04, 0x04,
        0x1f, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
        0x08, 0x08, 0x08, 0x08, 0x18, 0x08, 0x18, 0x08,
        0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
        0x10, 0x10, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
        0x04, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x08, 0x04, 0x08, 0x04, 0x04,
        0x04, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
        0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
        0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
        0x02, 0x02, 0x02, 0x08, 0x04, 0x08, 0x04, 0x04,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    static const guint8 upper[128] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        0x19, 0x1a, 0x1b, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    static const guint8 lower[128] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        0x19, 0x1a, 0x1b, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    static const guint8 mixed[128] = {
        0x00, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x0f, 0x10, 0x11, 0x12, 0x13,
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x15, 0x00, 0x16, 0x17,
        0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x19, 0x00, 0x1a, 0x1b
    };

    static const guint8 punct[128] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c,
        0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x1b, 0x00, 0x1c, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x1d, 0x00, 0x1e, 0x00, 0x00
    };

    static const guint8 digit[64] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x0d, 0x00,
        0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
        0x0a, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    AztecBlock* first_block = g_slice_new0(AztecBlock);
    AztecBlock* last_block = first_block;
    AztecBlock* block;
    AztecBuilder builder;
    const guint8* end = data + len;
    const guint8* ptr = data;

    /* Caller made sure that len > 0 */
    last_block->data = ptr;
    last_block->len = 1;
    last_block->mode = mode[*ptr++];

    /* Split data into blocks */
    while (ptr < end) {
        const guint8 c = *ptr++;
        guint8 m = mode[c];

        if (last_block->mode & MODE_PUNCT) {
            /*
             * As a special case, LF and SP in PUNCT mode can only appear
             * as the second symbol of a two-byte sequence:
             *
             * +------+----------+
             * | Code | Sequence |
             * +------+----------+
             * | 2    | CR LF    |
             * | 3    | . SP     |
             * | 4    | , SP     |
             * | 5    | : SP     |
             * +------+----------+
             */
            switch (c) {
            case LF:
                if (*(ptr - 2) == CR) {
                    last_block->len++;
                    continue;
                }
                m &= ~MODE_PUNCT;
                break;
            case SP:
                switch (*(ptr - 2)) {
                case '.':
                case ',':
                case ':':
                    last_block->len++;
                    continue;
                }
                m &= ~MODE_PUNCT;
                break;
            }
        }
        if (last_block->mode == m) {
            last_block->len++;
            continue;
        } else if (last_block->mode && (last_block->mode & m)) {
            /* Turn off mismatched bits */
            last_block->mode &= m;
            last_block->len++;
            continue;
        }

        /*
         * PUNCT mode can't start with LF or SP, those can only be the
         * second character in a sequence.
         */
        if (c == LF || c == SP) {
            m &= ~MODE_PUNCT;
        }

        /* Start next block */
        block = g_slice_new0(AztecBlock);
        block->data = ptr - 1;
        block->len = 1;
        block->mode = m;
        last_block->next = block;
        last_block = block;
    }

    /* Pick specific mode if more than one matched */
    for (block = first_block; block; block = block->next) {
        block->mode = modesubst[block->mode];
    }

    /* Try to enlarge Digit blocks */
    for (block = first_block; block && block->next; block = block->next) {
        AztecBlock* next = block->next;

        if (next->mode == MODE_DIGIT) {
            ptr = block->data + (block->len - 1);
            while (ptr > block->data && (mode[*ptr] & MODE_DIGIT)) {
                ptr--;
                next->data--;
                next->len++;
                block->len--;
            }
        }
    }

    /* Initialize the builder. The initial mode is Upper. */
    memset(&builder, 0, sizeof(builder));
    builder.bits = aztec_bits_new();
    builder.mode = MODE_UPPER;
    aztec_bits_reserve(builder.bits, len/2);

    /* Generate bitstream */
    for (block = first_block; block; block = block->next) {
        if (builder.pop_mode) {
            builder.mode = builder.pop_mode;
            builder.pop_mode = 0;
        }
        aztec_encode_builder_shift_or_latch(&builder, block);
        if (builder.mode == MODE_BINARY) {
            /*
             * Encoding long binary sequence may involve multiple
             * binary shifts.
             */
            aztec_encode_builder_append_binary_data(&builder, block->data);
            while (builder.binary_offset < block->len) {
                builder.mode = builder.pop_mode;
                aztec_encode_builder_shift_or_latch(&builder, block);
                aztec_encode_builder_append_binary_data(&builder, block->data);
            }
            builder.binary_offset = 0;
        } else if (builder.mode == MODE_PUNCT) {
            const guint nbits = 5;
            guint i;

            /*
             * Punct mode has 4 sequences:
             *
             * +------+----------+
             * | Code | Sequence |
             * +------+----------+
             * | 2    | CR LF    |
             * | 3    | . SP     |
             * | 4    | , SP     |
             * | 5    | : SP     |
             * +------+----------+
             */
            aztec_bits_reserve(builder.bits, /* OK to reserve slightly more */
                builder.bits->count + block->len * nbits);
            for (i = 0; (i + 1) < block->len; i++) {
                const guint c0 = block->data[i];
                const guint c1 = block->data[i + 1];

                if (c1 == SP) {
                    if (c0 == '.') {
                        aztec_encode_builder_add_bits(&builder, 3, nbits);
                        i++;
                        continue;
                    } else if (c0 == ',') {
                        aztec_encode_builder_add_bits(&builder, 4, nbits);
                        i++;
                        continue;
                    } else if (c0 == ':') {
                        aztec_encode_builder_add_bits(&builder, 5, nbits);
                        i++;
                        continue;
                    }
                } else if (c0 == CR && c1 == LF) {
                    aztec_encode_builder_add_bits(&builder, 2, nbits);
                    i++;
                    continue;
                }
                aztec_encode_builder_add_bits(&builder, punct[c0], nbits);
            }
            /* Last symbol */
            if (i < block->len) {
                aztec_encode_builder_add_bits(&builder, punct
                    [block->data[i]], nbits);
            }
        } else {
            /* The rest is handled mode or less identically */
            switch (block->mode) {
            case MODE_UPPER:
                aztec_encode_builder_append_data(&builder, block, upper, 5);
                break;
            case MODE_LOWER:
                aztec_encode_builder_append_data(&builder, block, lower, 5);
                break;
            case MODE_MIXED:
                aztec_encode_builder_append_data(&builder, block, mixed, 5);
                break;
            case MODE_DIGIT:
                aztec_encode_builder_append_data(&builder, block, digit, 4);
                break;
            }
        }
    }

    g_slice_free_chain(AztecBlock, first_block, next);
    return builder.bits;
}

static
AztecCodewords*
aztec_encode_codewords(
    AztecBits* bits,
    guint b)
{
    guint offset = 0;
    AztecCodewords* codewords = aztec_codewords_sized_new(bits->count / b);
    const guint ones = (1 << (b - 1)) - 1;

    while ((offset + b - 1) <= bits->count) {
        const guint word = aztec_bits_get_inv(bits, offset, b - 1);
        guint nextbit;

        /*
         * If the first b-1 bits of a code word have the same value,
         * an extra bit with the complementary value is inserted into
         * the data stream.
         */
        offset += b - 1;
        if (!word) {
            nextbit = 1;
        } else if (word == ones) {
            nextbit = 0;
        } else {
            if (offset < bits->count) {
                nextbit = aztec_bits_get_inv(bits, offset, 1);
                offset++;
            } else {
                nextbit = 1;
            }
        }
        aztec_codewords_append(codewords, (word << 1) | nextbit);
    }

    if (offset < bits->count) {
        const guint leftover = bits->count - offset;
        const guint pad = b - leftover;
        guint data = (aztec_bits_get_inv(bits, offset, leftover) << pad) |
            ((1 << pad) - 2); /* Leave last bit zero */

        if (data != (ones << 1)) {
            data |= 1;
        }

        aztec_codewords_append(codewords, data);
    }

    return codewords;
}

static
void
aztec_encode_mode_message(
    AztecBits* bits,
    guint16* words,
    guint mode_words,
    guint check_words)
{
    guint i;

    for (i = 0; i < mode_words; i++) {
        words[i] = aztec_bits_get_inv(bits, i * 4, 4);
    }
    aztec_rs_encode16_full(0x13, 1, words, mode_words,
        words + mode_words, check_words);
    aztec_bits_clear(bits);
    for (i = 0; i < (mode_words + check_words); i++) {
        aztec_bits_add_inv(bits, words[i], 4);
    }
}

static
AztecBits*
aztec_encode_compact_mode_message(
    guint layers,
    guint codewords)
{
    AztecBits* bits = aztec_bits_new();
    guint16 words[7];

    /* 28-bit mode message */
    aztec_bits_reserve(bits, 28);
    aztec_bits_add_inv(bits, layers - 1, 2);
    aztec_bits_add_inv(bits, codewords - 1, 6);
    aztec_encode_mode_message(bits, words, 2, 5);
    return bits;
}

static
AztecBits*
aztec_encode_full_mode_message(
    guint layers,
    guint codewords)
{
    AztecBits* bits = aztec_bits_new();
    guint16 words[10];

    /* 40-bit mode message */
    aztec_bits_reserve(bits, 40);
    aztec_bits_add_inv(bits, layers - 1, 5);
    aztec_bits_add_inv(bits, codewords - 1, 11);
    aztec_encode_mode_message(bits, words, 4, 6);
    return bits;
}

static
AztecBits*
aztec_encode_compact_symbol(
    guint symsize,
    AztecBits* data,
    AztecBits* mode)
{
    /*
     * Compact 11x11 core pattern (least significant bit first):
     *
     *  |0123456789A| Value  Mask
     * -+-----------+
     * 0|##        #| 0x0403 0x0603
     * 1|###########| 0x07ff 0x07ff
     * 2| #       # | 0x0202 0x03fe
     * 3| # ##### # | 0x02fa 0x03fe
     * 4| # #   # # | 0x028a 0x03fe
     * 5| # # # # # | 0x02aa 0x03fe
     * 6| # #   # # | 0x028a 0x03fe
     * 7| # ##### # | 0x02fa 0x03fe
     * 8| #       # | 0x0202 0x03fe
     * 9| ##########| 0x07fe 0x07ff
     * A|           | 0x0000 0x0603
     * -+-----------+
     */

    static const guint16 compact_core_data[] = {
        0x0403, 0x07ff, 0x0202, 0x02fa, 0x028a, 0x02aa, 0x028a, 0x02fa,
        0x0202, 0x07fe, 0x000
    };

    static const AztecPattern core = {
        G_N_ELEMENTS(compact_core_data),
        compact_core_data
    };

    AztecBits* symbol = aztec_bits_new();
    const guint core_offset = (symsize - core.size)/2;
    const guint layers = core_offset/2;
    guint k, l;
    int i;

    /* Fill the symbol with zeros. Bits are tightly packed! */
    aztec_bits_set(symbol, symsize * symsize, 0, 0);

    /* Core pattern */
    k = core_offset * (symsize + 1);
    for (i = 0; i < core.size; i++) {
        aztec_bits_set(symbol, k, core.data[i], core.size);
        k += symsize;
    }

    /*
     * Mode message (four 7-bit blocks clockwise starting from the
     * upper left corner):
     *
     *   ##------> #
     *   ###########
     *   ^#       #|
     *   |# ##### #|
     *   |# #   # #|
     *   |# # # # #|
     *   |# #   # #|
     *   |# ##### #|
     *   |#       #|
     *    ##########
     *     <------
     */

    /* Left to right */
    k = core_offset * (symsize + 1) + 2;
    aztec_bits_set(symbol, k, aztec_bits_get(mode, 0, 7), 7);

    /* Top to bottom */
    k += 2*symsize + 8;
    for (i = 7; i < 14; i++, k += symsize) {
        if (aztec_bits_get(mode, i, 1)) {
            aztec_bits_set(symbol, k, 1, 1);
        }
    }

    /* Right to left */
    k += symsize - 8;
    aztec_bits_set(symbol, k, aztec_bits_get_inv(mode, 14, 7), 7);

    /* Bottom to top */
    k = k - 2 - 2 * symsize;
    for (i = 21; i < 28; i++, k -= symsize) {
        if (aztec_bits_get(mode, i, 1)) {
            aztec_bits_set(symbol, k, 1, 1);
        }
    }

    /* Data layers (clockwise, least significant bits first) */

    for (l = 0, i = data->count - 1; l < layers; l++) {
        const guint n = core.size + 2 + 4 * l;
        guint x = core_offset - 2 * l;
        guint y = x - 1;
        guint k0 = y * symsize + x;

        /* Left to right */
        for (k = 0; k < n; k++, k0++, i -= 2) {
            const guint pair = aztec_bits_get(data, i - 1, 2);

            if (pair & 1) { /* More significant bit */
                aztec_bits_set(symbol, k0 - symsize, 1, 1);
            }
            if (pair & 2) { /* Less significant bit */
                aztec_bits_set(symbol, k0, 1, 1);
            }
        }

        /* Top to bottom */
        x = symsize - core_offset + 2 * l;
        y = core_offset - 2 * l;
        k0 = y * symsize + x;
        for (k = 0; k < n; k++, k0 += symsize, i -= 2) {
            const guint pair = aztec_bits_get(data, i - 1, 2);

            if (pair & 1) { /* More significant bit */
                aztec_bits_set(symbol, k0 + 1, 1, 1);
            }
            if (pair & 2) { /* Less significant bit */
                aztec_bits_set(symbol, k0, 1, 1);
            }
        }

        /* Right to left */
        y = x;
        x = y - 1;
        k0 = y * symsize + x;
        for (k = 0; k < n; k++, k0--, i -= 2) {
            const guint pair = aztec_bits_get(data, i - 1, 2);

            if (pair & 1) { /* More significant bit */
                aztec_bits_set(symbol, k0 + symsize, 1, 1);
            }
            if (pair & 2) { /* Less significant bit */
                aztec_bits_set(symbol, k0, 1, 1);
            }
        }

        /* Bottom to top */
        x = core_offset - 1 - 2 * l;
        y = symsize - core_offset -1 +  2 * l;
        k0 = y * symsize + x;
        for (k = 0; k < n && i > 0; k++, k0 -= symsize, i -= 2) {
            const guint pair = aztec_bits_get(data, i - 1, 2);

            if (pair & 1) { /* More significant bit */
                aztec_bits_set(symbol, k0 - 1, 1, 1);
            }
            if (pair & 2) { /* Less significant bit */
                aztec_bits_set(symbol, k0, 1, 1);
            }
        }
    }

    return symbol;
}

static
AztecBits*
aztec_encode_full_symbol(
    guint symsize,
    AztecBits* data,
    AztecBits* mode)
{
    /*
     * Full 15x15 core pattern (least significant bit first):
     *
     *  |0123456789ABCDE| Value  Mask
     * -+---------------+
     * 0|##            #| 0x4003 0x6003
     * 1|###############| 0x7fff 0x7fff
     * 2| #           # | 0x2002 0x3ffe
     * 3| # ######### # | 0x2ffa 0x3ffe
     * 4| # #       # # | 0x280a 0x3ffe
     * 5| # # ##### # # | 0x2bea 0x3ffe
     * 6| # # #   # # # | 0x2a2a 0x3ffe
     * 7| # # # # # # # | 0x2aaa 0x3ffe
     * 8| # # #   # # # | 0x2a2a 0x3ffe
     * 9| # # ##### # # | 0x2bea 0x3ffe
     * A| # #       # # | 0x280a 0x3ffe
     * B| # ######### # | 0x2ffa 0x3ffe
     * C| #           # | 0x2002 0x3ffe
     * D| ##############| 0x7ffe 0x7fff
     * E|               | 0x0000 0x6003
     * -+---------------+
     */

    static const guint16 full_core_data[] = {
        0x4003, 0x7fff, 0x2002, 0x2ffa, 0x280a, 0x2bea, 0x2a2a, 0x2aaa,
        0x2a2a, 0x2bea, 0x280a, 0x2ffa, 0x2002, 0x7ffe, 0x0000
    };

    static const AztecPattern core = {
        G_N_ELEMENTS(full_core_data),
        full_core_data
    };

    GByteArray* grid = g_byte_array_sized_new(symsize/16);
    AztecBits* symbol = aztec_bits_new();
    const int core_offset = (symsize - core.size)/2;
    const int center = symsize / 2;
    const guint layers = core_offset/2;
    guint i, k, l, xstart, ystart;
    guint8 byte;
    int j;

    /* Fill the symbol with zeros. Bits are tightly packed! */
    aztec_bits_set(symbol, symsize * symsize, 0, 0);

    /* Core pattern */
    k = core_offset * (symsize + 1);
    for (i = 0; i < core.size; i++) {
        aztec_bits_set(symbol, k, core.data[i], core.size);
        k += symsize;
    }

    /* Reference grid */
    for (j = core_offset - 1; j >= 0; j -= 2) {
        /* Top */
        aztec_bits_set(symbol, symsize * j + center, 1, 1);
        /* Bottom */
        aztec_bits_set(symbol, symsize * (symsize - j - 1) + center, 1, 1);
        /* Left */
        aztec_bits_set(symbol, symsize * center + j, 1, 1);
        /* Right */
        aztec_bits_set(symbol, symsize * (center + 1) - j - 1, 1, 1);
    }

    byte = (guint8)center;
    g_byte_array_append(grid, &byte, 1);
    for (j = center - 16; j >= 0; j -= 16) {
        guint k1 = symsize * j;
        guint k2 = symsize * (symsize - j - 1);
        guint k3 = j;
        guint k4 = symsize - j - 1;

        byte = (guint8)k3;
        g_byte_array_prepend(grid, &byte, 1);
        byte = (guint8)k4;
        g_byte_array_append(grid, &byte, 1);
        for (i = (center & 1); i < symsize; i += 2) {
            aztec_bits_set(symbol, k1 + i, 1, 1);
            aztec_bits_set(symbol, k2 + i, 1, 1);
            aztec_bits_set(symbol, k3 + i * symsize, 1, 1);
            aztec_bits_set(symbol, k4 + i * symsize, 1, 1);
        }
    }

    /*
     * Mode message (eight 5-bit blocks clockwise starting from the
     * upper left corner):
     *
     *   ##----> ----> #
     *   ###############
     *   ^#           #|
     *   |# ######### #|
     *   |# #       # #|
     *   |# # ##### # #|
     *   |# # #   # # #|
     *    # # # # # # #
     *   ^# # #   # # #|
     *   |# # ##### # #|
     *   |# #       # #|
     *   |# ######### #|
     *   |#           #|
     *    ##############
     *     <---- <----
     */

    /* Left to right */
    k = core_offset * (symsize + 1) + 2;
    aztec_bits_set(symbol, k, aztec_bits_get(mode, 0, 5), 5);
    aztec_bits_set(symbol, k + 6, aztec_bits_get(mode, 5, 5), 5);

    /* Top to bottom */
    k += 2*symsize + 12;
    for (i = 10; i < 15; i++, k += symsize) {
        if (aztec_bits_get(mode, i, 1)) {
            aztec_bits_set(symbol, k, 1, 1);
        }
    }
    for (k += symsize; i < 20; i++, k += symsize) {
        if (aztec_bits_get(mode, i, 1)) {
            aztec_bits_set(symbol, k, 1, 1);
        }
    }

    /* Right to left */
    k = k + symsize - 6;
    aztec_bits_set(symbol, k, aztec_bits_get_inv(mode, 20, 5), 5);
    aztec_bits_set(symbol, k - 6, aztec_bits_get_inv(mode, 25, 5), 5);

    /* Bottom to top */
    k = k - 2 * symsize - 8;
    for (i = 30; i < 35; i++, k -= symsize) {
        if (aztec_bits_get(mode, i, 1)) {
            aztec_bits_set(symbol, k, 1, 1);
        }
    }
    for (k -= symsize; i < 40; i++, k -= symsize) {
        if (aztec_bits_get(mode, i, 1)) {
            aztec_bits_set(symbol, k, 1, 1);
        }
    }

    /* Data layers (clockwise, least significant bits first) */

    xstart = core_offset + 2;
    ystart = core_offset + 1;

    for (l = 0, i = data->count - 1; l < layers; l++) {
        const guint n = core.size + 1 + 4 * l;
        int x, y, x0, x1, y0, y1;
        guint pair;

        xstart--;
        ystart--;
        if (aztec_bytes_contain(grid, xstart)) xstart--;
        if (aztec_bytes_contain(grid, ystart)) ystart--;
        xstart--;
        ystart--;
        if (aztec_bytes_contain(grid, xstart)) xstart--;
        if (aztec_bytes_contain(grid, ystart)) ystart--;

        /* Left to right */
        x0 = xstart;
        y0 = ystart;
        y1 = y0 - 1;
        if (aztec_bytes_contain(grid, y1)) y1--;

        for (k = 0, x = x0; k < n; k++, x++, i -= 2) {
            if (aztec_bytes_contain(grid, x)) x++;
            pair  = aztec_bits_get(data, i - 1, 2);
            if (pair & 2) { /* Less significant bit */
                aztec_bits_set(symbol, y0 * symsize + x, 1, 1);
            }
            if (pair & 1) { /* More significant bit */
                aztec_bits_set(symbol, y1 * symsize + x, 1, 1);
            }
        }

        /* Top to bottom */
        x1 = x - 1;
        x0 = x1 - 1;
        if (aztec_bytes_contain(grid, x0)) x0--;
        y0++;
        if (aztec_bytes_contain(grid, y0)) y0++;

        for (k = 0, y = y0; k < n; k++, y++, i -= 2) {
            if (aztec_bytes_contain(grid, y)) y++;
            pair = aztec_bits_get(data, i - 1, 2);
            if (pair & 2) { /* Less significant bit */
                aztec_bits_set(symbol, y * symsize + x0, 1, 1);
            }
            if (pair & 1) { /* More significant bit */
                aztec_bits_set(symbol, y * symsize + x1, 1, 1);
            }
        }

        /* Right to left */
        x0--;
        if (aztec_bytes_contain(grid, x0)) x0--;
        y1 = y - 1;
        y0 = y1 - 1;
        if (aztec_bytes_contain(grid, y0)) y0--;
        for (k = 0, x = x0; k < n; k++, x--, i -= 2) {
            if (aztec_bytes_contain(grid, x)) x--;
            pair  = aztec_bits_get(data, i - 1, 2);
            if (pair & 2) { /* Less significant bit */
                aztec_bits_set(symbol, y0 * symsize + x, 1, 1);
            }
            if (pair & 1) { /* More significant bit */
                aztec_bits_set(symbol, y1 * symsize + x, 1, 1);
            }
        }

        /* Bottom to top */
        x1 = x + 1;
        x0 = x1 + 1;
        if (aztec_bytes_contain(grid, x0)) x0++;
        y0--;
        if (aztec_bytes_contain(grid, y0)) y0--;
        for (k = 0, y = y0; k < n && i > 0; k++, y--, i -= 2) {
            if (aztec_bytes_contain(grid, y)) y--;
            pair = aztec_bits_get(data, i - 1, 2);
            if (pair & 2) { /* Less significant bit */
                aztec_bits_set(symbol, y * symsize + x0, 1, 1);
            }
            if (pair & 1) { /* More significant bit */
                aztec_bits_set(symbol, y * symsize + x1, 1, 1);
            }
        }
    }

    g_byte_array_unref(grid);
    return symbol;
}

static
gboolean
aztec_encode_pick_config(
    guint bitcount,
    guint correction,
    AztecConfig* config)
{
    static const AztecErrorCorrection all_errcor[] = {
        {
            10,
            { 78, 198, 336, 520 },
            { 96, 246, 408, 616, 840, 1104, 1392, 1704, 2040, 2420,
              2820, 3250, 3720, 4200, 4730, 5270, 5840, 6450, 7080, 7750,
              8430, 9150, 9900, 10680, 11484, 12324, 13188, 14076, 15000, 15948,
              16920, 17940 }
        },{
            23,
            { 66, 168, 288, 440 },
            { 84, 204, 352, 520, 720, 944, 1184, 1456, 1750, 2070,
              2410, 2780, 3180, 3590, 4040, 4500, 5000, 5520, 6060, 6630,
              7210, 7830, 8472, 9132, 9816, 10536, 11280, 12036, 12828, 13644,
              14472, 15348 }
        },{
            36,
            { 48, 138, 232, 360 },
            { 66, 168, 288, 432, 592, 776, 984, 1208, 1450, 1720,
              2000, 2300, 2640, 2980, 3350, 3740, 4150, 4580, 5030, 5500,
              5990, 6500, 7032, 7584, 8160, 8760, 9372, 9996, 10656, 11340,
              12024, 12744 }
        },{
            50,
            { 36, 102, 176, 280 },
            { 48, 126, 216, 328, 456, 600, 760, 936, 1120, 1330,
              1550, 1790, 2050, 2320, 2610, 2910, 3230, 3570, 3920, 4290,
              4670, 5070, 5484, 5916, 6360, 6828, 7308, 7800, 8316, 8844,
              9384, 9948 }
        }
    };

    /* Symbol paramters (Table 1) */
    static const AztecSymbolParams compact_symbols[MAX_COMPACT_LAYERS] = {
        { 15, 6, 17 }, { 19, 6, 40 },
        { 23, 8, 51 }, { 27, 8, 76 }
    };
    static const AztecSymbolParams full_symbols[MAX_FULL_LAYERS] = {
        { 19, 6, 21 }, { 23, 6, 48 },
        { 27, 8, 60 }, { 31, 8, 88 },
        { 37, 8, 120 }, { 41, 8, 156 },
        { 45, 8, 196 }, { 49, 8, 240 },
        { 53, 10, 230 }, { 57, 10, 272 },
        { 61, 10, 316 }, { 67, 10, 364 },
        { 71, 10, 416 }, { 75, 10, 470 },
        { 79, 10, 528 }, { 83, 10, 588 },
        { 87, 10, 652 }, { 91, 10, 720 },
        { 95, 10, 790 }, { 101, 10, 864 },
        { 105, 10, 940 }, { 109, 10, 1020 },
        { 113, 12, 920 }, { 117, 12, 992 },
        { 121, 12, 1066 }, { 125, 12, 1144 },
        { 131, 12, 1224 }, { 135, 12, 1306 },
        { 139, 12, 1392 }, { 143, 12, 1480 },
        { 147, 12, 1570 }, { 151, 12, 1664 }
    };

    const AztecErrorCorrection* errcor;
    const AztecSymbolParams* symbol = NULL;
    guint i;

    /* Pick error correction setup */
    for (i = 0; i < G_N_ELEMENTS(all_errcor); i++) {
        errcor = all_errcor + i;
        if (correction <= errcor->percent) {
            break;
        }
    }

    memset(config, 0, sizeof(*config));
    for (i = 0; i < G_N_ELEMENTS(errcor->compact); i++) {
        if (bitcount <= errcor->compact[i]) {
            symbol = compact_symbols + i;
            config->layers = i + 1;
            config->compact = TRUE;
            config->encode_mode_message = aztec_encode_compact_mode_message;
            config->encode_symbol = aztec_encode_compact_symbol;
            break;
        }
    }

    if (!symbol) {
        for (i = 0; i < G_N_ELEMENTS(errcor->full); i++) {
            if (bitcount <= errcor->full[i]) {
                symbol = full_symbols + i;
                config->layers = i + 1;
                config->encode_mode_message = aztec_encode_full_mode_message;
                config->encode_symbol = aztec_encode_full_symbol;
                break;
            }
        }
    }

    if (symbol) {
        config->symsize = symbol->size;
        config->cwcount = symbol->cwcount;
        config->cwsize = symbol->cwsize;
        /* Table 3 */
        switch (config->cwsize) {
        case 6: config->gfpoly = 0x43; break;
        case 8: config->gfpoly = 0x12d; break;
        case 10: config->gfpoly = 0x409; break;
        case 12: config->gfpoly = 0x1069; break;
        }
        return TRUE;
    } else {
        memset(config, 0, sizeof(*config));
        return FALSE;
    }
}

static
void
aztec_encode_symbol_fill_row(
    guint8* row,
    guint size,
    AztecBits* bits,
    int i)
{
    const gsize rowsize = (size + 7) / 8;
    guint x;

    for (x = 0; (x + 1) < rowsize; x++) {
        row[x] = aztec_bits_get(bits, i, 8);
        i += 8;
    }
    row[x] = aztec_bits_get(bits, i, size - x*8);
}

static
void
aztec_encode_symbol_fill_row_inv(
    guint8* row,
    guint size,
    AztecBits* bits,
    int i)
{
    const gsize rowsize = (size + 7) / 8;
    guint x, tail;

    for (x = 0; (x + 1) < rowsize; x++) {
        row[x] = aztec_bits_get_inv(bits, i, 8);
        i += 8;
    }
    tail = size - x*8;
    row[x] = aztec_bits_get_inv(bits, i, tail) << (8 - tail);
}

static
AztecSymbol*
aztec_encode_symbol_new(
    guint symsize,
    AztecBits* bits,
    AztecSymbolFillRowProc fill)
{
    const gsize rowsize = (symsize + 7) / 8;
    AztecSymbol* symbol = g_malloc(sizeof(AztecSymbol) +
        symsize * (sizeof(AztecSymbolRow) + rowsize));
    AztecSymbolRow* rows = (AztecSymbolRow*)(symbol + 1);
    guint8* data = (guint8*)(rows + symsize);
    guint y, i = 0;

    symbol->size = symsize;
    symbol->rows = rows;
    for (y = 0, i = 0; y < symsize; y++, i += symsize) {
        guint8* row = data + (y * rowsize);

        fill(row, symsize, bits, i);
        rows[y] = row;
    }
    return symbol;
}

void
aztec_symbol_free(
    AztecSymbol* symbol)
{
    g_free(symbol);
}

static
AztecSymbol*
aztec_encode_full(
    const void* data,
    gsize len,
    guint correction,
    AztecSymbolFillRowProc fill)
{
    AztecConfig config, config1;
    AztecCodewords* cw = NULL;
    AztecBits* bits = len ? aztec_encode_data_bits(data, len) :
        aztec_bits_new();
    guint bitcount = bits->count;
    AztecSymbol* symbol = NULL;

    memset(&config1, 0, sizeof(config1));
    while (aztec_encode_pick_config(bitcount, correction, &config) &&
        memcmp(&config, &config1, sizeof(config))) {
        aztec_codewords_free(cw, TRUE);
        cw = aztec_encode_codewords(bits, config.cwsize);
        bitcount = cw->count * config.cwsize;
        config1 = config;
    }

    if (config.layers) {
        const guint data_blocks = cw->count;
        const guint ecc_blocks = config.cwcount - data_blocks;
        AztecBits* mode_bits;
        AztecBits* symbol_bits;
        guint i;

        aztec_codewords_set_count(cw, config.cwcount);
        aztec_rs_encode16_full(config.gfpoly, 1, cw->words, data_blocks,
            cw->words + data_blocks, ecc_blocks);

        /* Repack codewords into a bitstream, most significant bit first */
        aztec_bits_clear(bits);
        aztec_bits_reserve(bits, cw->count * config.cwsize);
        for (i = 0; i < cw->count; i++) {
            aztec_bits_add_inv(bits, cw->words[i], config.cwsize);
        }

        /* Generate the symbol */
        mode_bits = config.encode_mode_message(config.layers, data_blocks);
        symbol_bits = config.encode_symbol(config.symsize, bits, mode_bits);
        aztec_bits_free(mode_bits);

        /* Convert the symbol into export format */
        symbol = aztec_encode_symbol_new(config.symsize, symbol_bits, fill);
        aztec_bits_free(symbol_bits);
    }
    aztec_codewords_free(cw, TRUE);
    aztec_bits_free(bits);
    return symbol;
}

AztecSymbol*
aztec_encode(
    const void* data,
    gsize len,
    guint correction)
{
    return aztec_encode_full(data, len, correction,
        aztec_encode_symbol_fill_row);
}

AztecSymbol*
aztec_encode_inv(
    const void* data,
    gsize len,
    guint correction) /* Since 1.0.2 */
{
    return aztec_encode_full(data, len, correction,
        aztec_encode_symbol_fill_row_inv);
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
