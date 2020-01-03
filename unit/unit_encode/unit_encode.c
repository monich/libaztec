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

#include <stdio.h>
#include <string.h>

static
void
test_print_symbol(
    const AztecSymbol* symbol)
{
    guint y;

    fprintf(stdout, "%ux%u\n", symbol->size, symbol->size);
    for (y = 0; y < symbol->size; y++) {
        const guint8* ptr = symbol->rows[y];
        guint i, x = 0;

        putc(' ', stdout);
        while (x < symbol->size) {
            guint b = *ptr++;
            for (i = 0; i < 8; i++, x++) {
                putc((b & 1) ? '#' : ' ', stdout);
                b >>= 1;
            }
        }
        putc('\n', stdout);
    }
}

static
void
test_check2(
    const AztecSymbol* symbol,
    const guint8 data[][2])
{
    guint i;

    if (g_test_verbose()) {
        test_print_symbol(symbol);
    }
    for (i = 0; i < symbol->size; i++) {
        g_assert(!memcmp(data[i], symbol->rows[i], 2));
    }
}

static
void
test_check3(
    const AztecSymbol* symbol,
    const guint8 data[][3])
{
    guint i;

    if (g_test_verbose()) {
        test_print_symbol(symbol);
    }
    for (i = 0; i < symbol->size; i++) {
        g_assert(!memcmp(data[i], symbol->rows[i], 3));
    }
}

static
void
test_check4(
    const AztecSymbol* symbol,
    const guint8 data[][4])
{
    guint i;

    if (g_test_verbose()) {
        test_print_symbol(symbol);
    }
    for (i = 0; i < symbol->size; i++) {
        g_assert(!memcmp(data[i], symbol->rows[i], 4));
    }
}

/* Code2D */

static
void
test_code2d(
    void)
{
    /* Example from ISO/IEC 24778 */
    static const char msg[] = "Code 2D!";
    static const guint8 data[15][2] = {
        { 0x18, 0x03 }, /*    ##   ##      */
        { 0xc0, 0x20 }, /*       ##     #  */
        { 0x0d, 0x51 }, /* # ##    #   # # */
        { 0xfe, 0x1f }, /*  ############   */
        { 0x0f, 0x58 }, /* ####       ## # */
        { 0xe8, 0x1b }, /*    # ##### ##   */
        { 0x29, 0x7a }, /* #  # #   # #### */
        { 0xac, 0x4a }, /*   ## # # # #  # */
        { 0x2c, 0x2a }, /*   ## #   # # #  */
        { 0xea, 0x4b }, /*  # # ##### #  # */
        { 0x09, 0x68 }, /* #  #       # ## */
        { 0xf9, 0x5f }, /* #  ########## # */
        { 0x62, 0x24 }, /*  #   ##   #  #  */
        { 0x86, 0x2d }, /*  ##    ## ## #  */
        { 0x67, 0x03 }  /* ###  ## ##      */
    };

    AztecSymbol* symbol =  aztec_encode(msg, sizeof(msg) - 1,
        AZTEC_CORRECTION_DEFAULT);

    g_assert(symbol);
    g_assert(symbol->size == G_N_ELEMENTS(data));
    test_check2(symbol, data);
    aztec_symbol_free(symbol);
}

/* Test */

static
void
test_test(
    void)
{
    static const char msg[] = "test";
    static const guint8 data[15][2] = {
        { 0x2c, 0x56 }, /*   ## #   ## # # */
        { 0xbb, 0x5a }, /* ## ### # # ## # */
        { 0x0d, 0x12 }, /* # ##     #  #   */
        { 0xfe, 0x1f }, /*  ############   */
        { 0x0a, 0x08 }, /*  # #       #    */
        { 0xea, 0x3b }, /*  # # ##### ###  */
        { 0x2c, 0x2a }, /*   ## #   # # #  */
        { 0xab, 0x5a }, /* ## # # # # ## # */
        { 0x2a, 0x6a }, /*  # # #   # # ## */
        { 0xea, 0x0b }, /*  # # ##### #    */
        { 0x08, 0x08 }, /*    #       #    */
        { 0xf9, 0x3f }, /* #  ###########  */
        { 0x71, 0x27 }, /* #   ### ###  #  */
        { 0xf3, 0x24 }, /* ##  ####  #  #  */
        { 0xd7, 0x29 }  /* ### # ###  # #  */
    };

    AztecSymbol* symbol =  aztec_encode(msg, sizeof(msg) - 1,
        AZTEC_CORRECTION_DEFAULT);

    g_assert(symbol);
    g_assert(symbol->size == G_N_ELEMENTS(data));
    test_check2(symbol, data);
    aztec_symbol_free(symbol);
}

/* Test (inv) */

static
void
test_test_inv(
    void)
{
    static const char msg[] = "test";
    static const guint8 data[15][2] = {
        { 0x34, 0x6a }, /*   ## #   ## # # */
        { 0xdd, 0x5a }, /* ## ### # # ## # */
        { 0xb0, 0x48 }, /* # ##     #  #   */
        { 0x7f, 0xf8 }, /*  ############   */
        { 0x50, 0x10 }, /*  # #       #    */
        { 0x57, 0xdc }, /*  # # ##### ###  */
        { 0x34, 0x54 }, /*   ## #   # # #  */
        { 0xd5, 0x5a }, /* ## # # # # ## # */
        { 0x54, 0x56 }, /*  # # #   # # ## */
        { 0x57, 0xd0 }, /*  # # ##### #    */
        { 0x10, 0x10 }, /*    #       #    */
        { 0x9f, 0xfc }, /* #  ###########  */
        { 0x8e, 0xe4 }, /* #   ### ###  #  */
        { 0xcf, 0x24 }, /* ##  ####  #  #  */
        { 0xeb, 0x94 }  /* ### # ###  # #  */
    };

    AztecSymbol* symbol =  aztec_encode_inv(msg, sizeof(msg) - 1,
        AZTEC_CORRECTION_DEFAULT);

    g_assert(symbol);
    g_assert(symbol->size == G_N_ELEMENTS(data));
    test_check2(symbol, data);
    aztec_symbol_free(symbol);
}

/* E-mail */

static
void
test_email(
    void)
{
    static const char msg[] = "slava@monich.com";
    static const guint8 data[19][3] = {
        { 0x33, 0xb3, 0x01 }, /* ##  ##  ##  ## ##   */
        { 0xad, 0xac, 0x04 }, /* # ## # #  ## # #  # */
        { 0x9a, 0xc7, 0x07 }, /*  # ##  ####   ##### */
        { 0xa6, 0x3a, 0x04 }, /*  ##  # # # ###    # */
        { 0xb8, 0x42, 0x07 }, /*    ### # #    # ### */
        { 0xfa, 0xff, 0x00 }, /*  # #############    */
        { 0x35, 0x20, 0x00 }, /* # # ##       #      */
        { 0xa5, 0x6f, 0x04 }, /* # #  # ##### ##   # */
        { 0xa0, 0x68, 0x01 }, /*      # #   # ## #   */
        { 0xa9, 0xaa, 0x03 }, /* #  # # # # # # ###  */
        { 0xbd, 0xa8, 0x05 }, /* # #### #   # # ## # */
        { 0xb3, 0xef, 0x02 }, /* ##  ## ##### ### #  */
        { 0x39, 0x20, 0x04 }, /* #  ###       #    # */
        { 0xe0, 0x7f, 0x01 }, /*      ########## #   */
        { 0x0d, 0x0e, 0x00 }, /* # ##     ###        */
        { 0xab, 0x5c, 0x05 }, /* ## # # #  ### # # # */
        { 0x11, 0x77, 0x01 }, /* #   #   ### ### #   */
        { 0x4b, 0x78, 0x03 }, /* ## #  #    #### ##  */
        { 0xd9, 0x19, 0x00 }  /* #  ## ###  ##       */
    };

    AztecSymbol* symbol =  aztec_encode(msg, sizeof(msg) - 1, 100);

    g_assert(symbol);
    g_assert(symbol->size == G_N_ELEMENTS(data));
    test_check3(symbol, data);
    aztec_symbol_free(symbol);
}

/* Upper */
static
void
test_upper(
    void)
{
    /* N.B. NULL terminator is included in the message */
    static const char msg[] = "lowUP@UP__UP1UP*UP()UP";
    static const guint8 data[19][3] = {
        { 0xdb, 0x68, 0x06 }, /* ## ## ##   # ##  ## */
        { 0xe1, 0x81, 0x07 }, /* #    ####      #### */
        { 0xac, 0xb1, 0x02 }, /*   ## # ##   ## # #  */
        { 0x5f, 0x61, 0x01 }, /* ##### # #    ## #   */
        { 0xba, 0x5e, 0x07 }, /*  # ### # #### # ### */
        { 0xfd, 0xff, 0x06 }, /* # ############## ## */
        { 0x2c, 0xe0, 0x03 }, /*   ## #       #####  */
        { 0xb2, 0xef, 0x00 }, /*  #  ## ##### ###    */
        { 0xbd, 0xe8, 0x03 }, /* # #### #   # #####  */
        { 0xb8, 0xea, 0x07 }, /*    ### # # # ###### */
        { 0xab, 0x68, 0x04 }, /* ## # # #   # ##   # */
        { 0xbf, 0xaf, 0x02 }, /* ###### ##### # # #  */
        { 0x2a, 0x20, 0x07 }, /*  # # #       #  ### */
        { 0xef, 0xff, 0x06 }, /* #### ########### ## */
        { 0x4e, 0x84, 0x06 }, /*  ###  #   #    # ## */
        { 0xda, 0x89, 0x05 }, /*  # ## ###  #   ## # */
        { 0x8d, 0x03, 0x06 }, /* # ##   ###       ## */
        { 0x9c, 0xdb, 0x02 }, /*   ###  ### ## ## #  */
        { 0x6d, 0x8d, 0x07 }  /* # ## ## # ##   #### */
    };

    AztecSymbol* symbol =  aztec_encode(msg, sizeof(msg), 0);

    g_assert(symbol);
    g_assert(symbol->size == G_N_ELEMENTS(data));
    test_check3(symbol, data);
    aztec_symbol_free(symbol);
}

/* Lower */

static
void
test_lower(
    void)
{
    /* N.B. NULL terminator is included in the message */
    static const char msg[] = "lowUlowUPlow_low!low.,low0low";
    static const guint8 data[23][3] = {
        { 0x3f, 0xfd, 0x52 }, /* ######  # ###### #  # # */
        { 0x79, 0xb3, 0x64 }, /* #  #### ##  ## #  #  ## */
        { 0x94, 0x1c, 0x03 }, /*   # #  #  ###   ##      */
        { 0xe3, 0x33, 0x71 }, /* ##   #####  ##  #   ### */
        { 0x3e, 0x00, 0x08 }, /*  #####             #    */
        { 0x69, 0x99, 0x34 }, /* #  # ## #  ##  #  # ##  */
        { 0xc4, 0x39, 0x07 }, /*   #   ###  ###  ###     */
        { 0xc2, 0xff, 0x19 }, /*  #    ###########  ##   */
        { 0xc9, 0x80, 0x15 }, /* #  #  ##       ## # #   */
        { 0x84, 0xbe, 0x4b }, /*   #    # ##### ### #  # */
        { 0xa3, 0xa2, 0x24 }, /* ##   # # #   # #  #  #  */
        { 0x9d, 0xaa, 0x08 }, /* # ###  # # # # #   #    */
        { 0xee, 0xa2, 0x4f }, /*  ### ### #   # #####  # */
        { 0xd2, 0xbe, 0x26 }, /*  #  # ## ##### # ##  #  */
        { 0xa9, 0x80, 0x69 }, /* #  # # #       ##  # ## */
        { 0xaa, 0xff, 0x09 }, /*  # # # ##########  #    */
        { 0x2d, 0x6f, 0x56 }, /* # ## #  #### ##  ## # # */
        { 0x4b, 0x8e, 0x70 }, /* ## #  #  ###   #    ### */
        { 0xac, 0x1d, 0x7d }, /*   ## # ## ###   # ##### */
        { 0x28, 0x1d, 0x38 }, /*    # #  # ###      ###  */
        { 0x33, 0x99, 0x68 }, /* ##  ##  #  ##  #   # ## */
        { 0x96, 0xcd, 0x44 }, /*  ## #  ## ##  ##  #   # */
        { 0xfc, 0x58, 0x69 }  /*   ######   ## # #  # ## */
    };
    AztecSymbol* symbol =  aztec_encode(msg, sizeof(msg), 0);

    g_assert(symbol);
    g_assert(symbol->size == G_N_ELEMENTS(data));
    test_check3(symbol, data);
    aztec_symbol_free(symbol);
}

/* Mixed */

static
void
test_mixed(
    void)
{
    /* N.B. NULL terminator is included in the message */
    static const char msg[] = "|Up_lo^.@. @1^";
    static const guint8 data[19][3] = {
        { 0xef, 0xb7, 0x01 }, /* #### ###### ## ##   */
        { 0xb5, 0x66, 0x05 }, /* # # ## # ##  ## # # */
        { 0xff, 0x86, 0x07 }, /* ######## ##    #### */
        { 0x55, 0x33, 0x05 }, /* # # # # ##  ##  # # */
        { 0xb2, 0xc6, 0x06 }, /*  #  ## # ##   ## ## */
        { 0xfb, 0xff, 0x05 }, /* ## ############## # */
        { 0x21, 0xe0, 0x06 }, /* #    #       ### ## */
        { 0xa3, 0x2f, 0x07 }, /* ##   # ##### #  ### */
        { 0xaa, 0xa8, 0x06 }, /*  # # # #   # # # ## */
        { 0xb1, 0xaa, 0x00 }, /* #   ## # # # # #    */
        { 0xab, 0xa8, 0x00 }, /* ## # # #   # # #    */
        { 0xbd, 0x6f, 0x06 }, /* # #### ##### ##  ## */
        { 0x32, 0x20, 0x07 }, /*  #  ##       #  ### */
        { 0xe4, 0x7f, 0x01 }, /*   #  ########## #   */
        { 0x8e, 0x9f, 0x03 }, /*  ###   ######  ###  */
        { 0x1b, 0xb7, 0x06 }, /* ## ##   ### ## # ## */
        { 0x7d, 0x66, 0x03 }, /* # #####  ##  ## ##  */
        { 0xcf, 0x5c, 0x04 }, /* ####  ##  ### #   # */
        { 0x5d, 0x69, 0x00 }  /* # ### # #  # ##     */
    };
    AztecSymbol* symbol =  aztec_encode(msg, sizeof(msg), 0);

    g_assert(symbol);
    g_assert(symbol->size == G_N_ELEMENTS(data));
    test_check3(symbol, data);
    aztec_symbol_free(symbol);
}

/* Punct1 */

static
void
test_punct1(
    void)
{
    static const char msg[] = ", ";
    static const guint8 data[15][2] = {
        { 0x40, 0x3d }, /*       # # ####  */
        { 0xbb, 0x50 }, /* ## ### #    # # */
        { 0x0d, 0x14 }, /* # ##      # #   */
        { 0xff, 0x1f }, /* #############   */
        { 0x0b, 0x68 }, /* ## #       # ## */
        { 0xed, 0x0b }, /* # ## ##### #    */
        { 0x28, 0x1a }, /*    # #   # ##   */
        { 0xa9, 0x2a }, /* #  # # # # # #  */
        { 0x2a, 0x3a }, /*  # # #   # ###  */
        { 0xeb, 0x1b }, /* ## # ##### ##   */
        { 0x0e, 0x08 }, /*  ###       #    */
        { 0xf9, 0x3f }, /* #  ###########  */
        { 0x82, 0x01 }, /*  #     ##       */
        { 0xdb, 0x2d }, /* ## ## ### ## #  */
        { 0x35, 0x6e }  /* # # ##   ### ## */
    };
    AztecSymbol* symbol =  aztec_encode(msg, sizeof(msg) - 1, 0);

    g_assert(symbol);
    g_assert(symbol->size == G_N_ELEMENTS(data));
    test_check2(symbol, data);
    aztec_symbol_free(symbol);
}

/* Punct2 */

static
void
test_punct2(
    void)
{
    static const char msg[] = {'\xd0', '\xb0', ',', ' ', '\xd0', '\xb0' };
    static const guint8 data[15][2] = {
        { 0xb0, 0x69 }, /*     ## ##  # ## */
        { 0xab, 0x40 }, /* ## # # #      # */
        { 0x0f, 0x33 }, /* ####    ##  ##  */
        { 0xfd, 0x1f }, /* # ###########   */
        { 0x08, 0x48 }, /*    #       #  # */
        { 0xea, 0x3b }, /*  # # ##### ###  */
        { 0x2e, 0x3a }, /*  ### #   # ###  */
        { 0xad, 0x0a }, /* # ## # # # #    */
        { 0x2d, 0x5a }, /* # ## #   # ## # */
        { 0xec, 0x4b }, /*   ## ##### #  # */
        { 0x0a, 0x38 }, /*  # #       ###  */
        { 0xfa, 0x3f }, /*  # ###########  */
        { 0xd1, 0x03 }, /* #   # ####      */
        { 0xbe, 0x56 }, /*  ##### # ## # # */
        { 0x34, 0x7e }  /*   # ##   ###### */
    };
    AztecSymbol* symbol =  aztec_encode(msg, sizeof(msg), 0);

    g_assert(symbol);
    g_assert(symbol->size == G_N_ELEMENTS(data));
    test_check2(symbol, data);
    aztec_symbol_free(symbol);
}

/* Punct3 */

static
void
test_punct3(
    void)
{
    static const char msg[] = "a,\na";
    static const guint8 data[15][2] = {
        { 0xc8, 0x76 }, /*    #  ## ## ### */
        { 0x63, 0x1e }, /* ##   ##  ####   */
        { 0x0d, 0x76 }, /* # ##     ## ### */
        { 0xfc, 0x7f }, /*   ############# */
        { 0x08, 0x28 }, /*    #       # #  */
        { 0xed, 0x7b }, /* # ## ##### #### */
        { 0x2c, 0x7a }, /*   ## #   # #### */
        { 0xa8, 0x7a }, /*    # # # # #### */
        { 0x2a, 0x1a }, /*  # # #   # ##   */
        { 0xe8, 0x7b }, /*    # ##### #### */
        { 0x0e, 0x28 }, /*  ###       # #  */
        { 0xfb, 0x3f }, /* ## ###########  */
        { 0xf1, 0x06 }, /* #   #### ##     */
        { 0x0c, 0x79 }, /*   ##    #  #### */
        { 0x9f, 0x39 }, /* #####  ##  ###  */
    };
    AztecSymbol* symbol =  aztec_encode(msg, sizeof(msg) - 1, 0);

    g_assert(symbol);
    g_assert(symbol->size == G_N_ELEMENTS(data));
    test_check2(symbol, data);
    aztec_symbol_free(symbol);
}

/* Punct4 */

static
void
test_punct4(
    void)
{
    static const char msg[] = "$\r\n\r.UP: lo$. @$! , $0++";
    static const guint8 data[19][3] = {
        { 0x97, 0xe6, 0x07 }, /* ### #  # ##  ###### */
        { 0x5d, 0xb6, 0x06 }, /* # ### #  ## ## # ## */
        { 0xef, 0x2f, 0x06 }, /* #### ####### #   ## */
        { 0x3f, 0x44, 0x00 }, /* ######    #   #     */
        { 0xbd, 0x5e, 0x04 }, /* # #### # #### #   # */
        { 0xf2, 0x7f, 0x01 }, /*  #  ########### #   */
        { 0x20, 0x60, 0x02 }, /*      #       ##  #  */
        { 0xbd, 0x6f, 0x05 }, /* # #### ##### ## # # */
        { 0xb4, 0x68, 0x07 }, /*   # ## #   # ## ### */
        { 0xb1, 0xea, 0x07 }, /* #   ## # # # ###### */
        { 0xa0, 0xe8, 0x05 }, /*      # #   # #### # */
        { 0xb4, 0x2f, 0x07 }, /*   # ## ##### #  ### */
        { 0x23, 0x20, 0x00 }, /* ##   #       #      */
        { 0xe0, 0xff, 0x00 }, /*      ###########    */
        { 0x47, 0x04, 0x06 }, /* ###   #   #      ## */
        { 0xa7, 0x46, 0x04 }, /* ###  # # ##   #   # */
        { 0xf3, 0x1f, 0x02 }, /* ##  #########    #  */
        { 0x6c, 0x63, 0x01 }, /*   ## ## ##   ## #   */
        { 0xc7, 0xf7, 0x07 }  /* ###   ##### ####### */
    };
    AztecSymbol* symbol =  aztec_encode(msg, sizeof(msg) - 1, 0);

    g_assert(symbol);
    g_assert(symbol->size == G_N_ELEMENTS(data));
    test_check3(symbol, data);
    aztec_symbol_free(symbol);
}

/* Digit */

static
void
test_digit(
    void)
{
    /* N.B. NULL terminator is included in the message */
    static const char msg[] = "1U2UP3l4@5!6$$7";
    static const guint8 data[19][3] = {
        { 0x37, 0x7f, 0x03 }, /* ### ##  ####### ##  */
        { 0x8f, 0x56, 0x04 }, /* ####   # ## # #   # */
        { 0x6c, 0x88, 0x02 }, /*   ## ##    #   # #  */
        { 0x4e, 0x47, 0x01 }, /*  ###  # ###   # #   */
        { 0xbf, 0xce, 0x01 }, /* ###### # ###  ###   */
        { 0xf7, 0x7f, 0x06 }, /* ### ###########  ## */
        { 0x3b, 0xa0, 0x03 }, /* ## ###       # ###  */
        { 0xa2, 0x2f, 0x04 }, /*  #   # ##### #    # */
        { 0xbd, 0x28, 0x06 }, /* # #### #   # #   ## */
        { 0xbe, 0xaa, 0x03 }, /*  ##### # # # # ###  */
        { 0xa8, 0x68, 0x05 }, /*    # # #   # ## # # */
        { 0xaf, 0xef, 0x07 }, /* #### # ##### ###### */
        { 0x3d, 0xe0, 0x01 }, /* # ####       ####   */
        { 0xe1, 0xff, 0x02 }, /* #    ########### #  */
        { 0x43, 0x01, 0x06 }, /* ##    # #        ## */
        { 0xc2, 0x61, 0x04 }, /*  #    ###    ##   # */
        { 0x84, 0xb0, 0x07 }, /*   #    #    ## #### */
        { 0xe7, 0xb5, 0x04 }, /* ###  #### # ## #  # */
        { 0x76, 0x79, 0x07 }  /*  ## ### #  #### ### */
    };
    AztecSymbol* symbol =  aztec_encode(msg, sizeof(msg), 0);

    g_assert(symbol);
    g_assert(symbol->size == G_N_ELEMENTS(data));
    test_check3(symbol, data);
    aztec_symbol_free(symbol);
}

/* Compact4 */

static
void
test_compact4(
    void)
{
    static const char msg[] = "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQq";
    static const guint8 data[27][4] = {
        { 0xa8, 0xed, 0x94, 0x06 }, /*    # # ## ## ###  # #  # ## */
        { 0x7a, 0x39, 0xad, 0x04 }, /*  # #### #  ###  # ## # #  # */
        { 0x2a, 0x15, 0x93, 0x06 }, /*  # # #  # # #   ##  #  # ## */
        { 0xff, 0xac, 0x17, 0x04 }, /* ########  ## # #### #     # */
        { 0xc4, 0x55, 0xdf, 0x05 }, /*   #   ### # # # ##### ### # */
        { 0x0c, 0x12, 0xb4, 0x02 }, /*   ##     #  #     # ## # #  */
        { 0xce, 0xd8, 0xdc, 0x03 }, /*  ###  ##   ## ##  ### ####  */
        { 0xce, 0xba, 0xe2, 0x01 }, /*  ###  ## # ### # #   ####   */
        { 0xd7, 0xef, 0x4d, 0x07 }, /* ### # ###### #### ##  # ### */
        { 0x98, 0xff, 0x3f, 0x02 }, /*    ##  ###############   #  */
        { 0xf0, 0x03, 0xde, 0x03 }, /*     ######       #### ####  */
        { 0x92, 0xfa, 0x3e, 0x02 }, /*  #  #  # # ##### #####   #  */
        { 0xf5, 0x8a, 0x9e, 0x05 }, /* # # #### # #   # ####  ## # */
        { 0x90, 0xab, 0x92, 0x05 }, /*     #  ### # # # #  #  ## # */
        { 0x0b, 0x8a, 0x2e, 0x00 }, /* ## #     # #   # ### #      */
        { 0x2f, 0xfa, 0x56, 0x05 }, /* #### #   # ##### ## # # # # */
        { 0xc1, 0x03, 0x1a, 0x07 }, /* #     ####       # ##   ### */
        { 0x54, 0xfe, 0x37, 0x02 }, /*   # # #  ########## ##   #  */
        { 0x92, 0x54, 0x81, 0x05 }, /*  #  #  #  # # # #      ## # */
        { 0xec, 0x93, 0xed, 0x05 }, /*   ## #####  #  ## ## #### # */
        { 0x20, 0xcb, 0x3e, 0x01 }, /*      #  ## #  ## #####  #   */
        { 0xd9, 0xec, 0xb4, 0x03 }, /* #  ## ##  ## ###  # ## ###  */
        { 0x8e, 0xb6, 0x4c, 0x01 }, /*  ###   # ## ## #  ##  # #   */
        { 0x8b, 0x35, 0xa0, 0x07 }, /* ## #   ## # ##       # #### */
        { 0x64, 0xa6, 0xde, 0x02 }, /*   #  ##  ##  # # #### ## #  */
        { 0x38, 0x39, 0x1d, 0x00 }, /*    ###  #  ###  # ###       */
        { 0x66, 0x2a, 0xbb, 0x02 }  /*  ##  ##  # # #  ## ### # #  */
    };
    AztecSymbol* symbol =  aztec_encode(msg, sizeof(msg) - 1, 100);

    g_assert(symbol);
    g_assert(symbol->size == G_N_ELEMENTS(data));
    test_check4(symbol, data);
    aztec_symbol_free(symbol);
}

/* Full4 */

static
void
test_full4(
    void)
{
    static const char msg[] = "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSs";
    AztecSymbol* symbol =  aztec_encode(msg, sizeof(msg) - 1, 100);

    g_assert(symbol);
    if (g_test_verbose()) {
        test_print_symbol(symbol);
    }
    //g_assert(symbol->size == G_N_ELEMENTS(data));
    //test_check4(symbol, data);
    aztec_symbol_free(symbol);
}

/* 400x1 */

static
void
test_400x1(
    void)
{
    static const char msg[] =
        "11111111111111111111111111111111111111111111111111"
        "11111111111111111111111111111111111111111111111111"
        "11111111111111111111111111111111111111111111111111"
        "11111111111111111111111111111111111111111111111111"
        "11111111111111111111111111111111111111111111111111"
        "11111111111111111111111111111111111111111111111111"
        "11111111111111111111111111111111111111111111111111"
        "11111111111111111111111111111111111111111111111111";
    AztecSymbol* symbol =  aztec_encode(msg, sizeof(msg) - 1,
        AZTEC_CORRECTION_MEDIUM);

    g_assert(symbol);
    if (g_test_verbose()) {
        test_print_symbol(symbol);
    }
    aztec_symbol_free(symbol);
}

/* 400x3 */

static
void
test_400x3(
    void)
{
    static const char msg[] =
        "33333333333333333333333333333333333333333333333333"
        "33333333333333333333333333333333333333333333333333"
        "33333333333333333333333333333333333333333333333333"
        "33333333333333333333333333333333333333333333333333"
        "33333333333333333333333333333333333333333333333333"
        "33333333333333333333333333333333333333333333333333"
        "33333333333333333333333333333333333333333333333333"
        "33333333333333333333333333333333333333333333333333";
    AztecSymbol* symbol =  aztec_encode(msg, sizeof(msg) - 1,
        AZTEC_CORRECTION_MEDIUM);

    g_assert(symbol);
    if (g_test_verbose()) {
        test_print_symbol(symbol);
    }
    aztec_symbol_free(symbol);
}

/* toomuch */

static
void
test_toomuch(
    void)
{
    static const char msg[] =
        "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz"
        "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz"
        "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz"
        "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz"
        "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz"
        "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz"
        "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz"
        "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz"
        "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz"
        "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz"
        "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz"
        "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz"
        "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz"
        "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz"
        "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz"
        "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz"
        "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz"
        "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz"
        "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz"
        "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz"
        "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz"
        "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz"
        "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz"
        "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz"
        "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz"
        "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz";

    g_assert(!aztec_encode(msg, sizeof(msg) - 1, AZTEC_CORRECTION_HIGHEST));
}

/* Binary */

static
void
test_binary1(
    void)
{
    static const char msg[60] = { 0 };
    AztecSymbol* symbol =  aztec_encode(msg, sizeof(msg),
        AZTEC_CORRECTION_MEDIUM);

    g_assert(symbol);
    if (g_test_verbose()) {
        test_print_symbol(symbol);
    }
    aztec_symbol_free(symbol);
}

static
void
test_binary2(
    void)
{
    static const char msg[100] = { 0 };
    AztecSymbol* symbol =  aztec_encode(msg, sizeof(msg),
        AZTEC_CORRECTION_MEDIUM);

    g_assert(symbol);
    if (g_test_verbose()) {
        test_print_symbol(symbol);
    }
    aztec_symbol_free(symbol);
}

/* Common */

#define TEST_(x) "/encode/" x

int main(int argc, char* argv[])
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func(TEST_("code2d"), test_code2d);
    g_test_add_func(TEST_("test"), test_test);
    g_test_add_func(TEST_("test_inv"), test_test_inv);
    g_test_add_func(TEST_("email"), test_email);
    g_test_add_func(TEST_("upper"), test_upper);
    g_test_add_func(TEST_("lower"), test_lower);
    g_test_add_func(TEST_("mixed"), test_mixed);
    g_test_add_func(TEST_("punct1"), test_punct1);
    g_test_add_func(TEST_("punct2"), test_punct2);
    g_test_add_func(TEST_("punct3"), test_punct3);
    g_test_add_func(TEST_("punct4"), test_punct4);
    g_test_add_func(TEST_("digit"), test_digit);
    g_test_add_func(TEST_("compact4"), test_compact4);
    g_test_add_func(TEST_("full4"), test_full4);
    g_test_add_func(TEST_("400x1"), test_400x1);
    g_test_add_func(TEST_("400x3"), test_400x3);
    g_test_add_func(TEST_("toomuch"), test_toomuch);
    g_test_add_func(TEST_("binary1"), test_binary1);
    g_test_add_func(TEST_("binary2"), test_binary2);
    return g_test_run();
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
