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

/* Basic */

static
void
test_basic(
    void)
{
    AztecBits* bits = aztec_bits_new();

    g_assert(bits);
    g_assert(!bits->count);
    aztec_bits_reserve(bits, 0);
    aztec_bits_reserve(bits, 1);

    /* Add nothing */
    aztec_bits_add(bits, 0, 0);
    g_assert(!bits->count);

    /* Add 2 bits */
    aztec_bits_add(bits, 2, 2);
    g_assert(bits->count == 2);

    /* Check the bits */
    g_assert(aztec_bits_get(bits, 0, 1) == 0);
    g_assert(aztec_bits_get(bits, 1, 1) == 1);

    /* Invalid queries return zero */
    g_assert(!aztec_bits_get(bits, 1, 0));
    g_assert(!aztec_bits_get(bits, 2, 1));

    /* Can't add more than 32 bits at once */
    aztec_bits_add(bits, 0, 33);
    g_assert(bits->count == 34);

    aztec_bits_free(bits);
}

/* Scatter */

static
void
test_scatter(
    void)
{
    AztecBits* bits = aztec_bits_new();

    aztec_bits_add(bits, 0, 31);

    /* These 2 bits cross the unit boundary */
    aztec_bits_add(bits, 3, 2);

    /* Pull them back */
    g_assert(aztec_bits_get(bits, 31, 2) == 3);
    g_assert(aztec_bits_get(bits, 31, 3) == 3);
    g_assert(aztec_bits_get(bits, 0, 33) == 0x80000000);

    /* Can't add more than 32 bits at once */
    aztec_bits_add_inv(bits, 0x00ff00ff, 33);
    g_assert(bits->count == 65);
    g_assert(aztec_bits_get(bits, 33, 33) == 0xff00ff00);

    /* Get inverted values */
    g_assert(aztec_bits_get_inv(bits, 0, 33) == 1);
    g_assert(aztec_bits_get_inv(bits, 31, 1) == 1);
    g_assert(aztec_bits_get_inv(bits, 33, 8) == 0);
    g_assert(aztec_bits_get_inv(bits, 41, 8) == 0xff);
    g_assert(aztec_bits_get_inv(bits, 33, 33) == 0x00ff00ff);

    aztec_bits_free(bits);
}

/* Invert */

static
void
test_invert(
    void)
{
    AztecBits* bits = aztec_bits_new();

    aztec_bits_add_inv(bits, 0, 0);

    /* These will be inverted */
    aztec_bits_add_inv(bits, 2, 2);
    aztec_bits_add_inv(bits, 1, 2);
    g_assert(bits->count == 4);

    /* Check the inverted bits */
    g_assert(aztec_bits_get(bits, 0, 32) == 0x09);
    g_assert(aztec_bits_get(bits, 0, 1) == 1);
    g_assert(aztec_bits_get(bits, 1, 1) == 0);
    g_assert(aztec_bits_get(bits, 2, 1) == 0);
    g_assert(aztec_bits_get(bits, 3, 1) == 1);

    /* These don't actually need to be inverted */
    aztec_bits_add_inv(bits, 1, 1);
    aztec_bits_add_inv(bits, 0, 1);
    aztec_bits_add_inv(bits, 3, 2);
    aztec_bits_add_inv(bits, 0, 2);
    g_assert(bits->count == 10);

    /* Check the bits */
    g_assert(aztec_bits_get(bits, 0, 32) == 0xd9);
    g_assert(aztec_bits_get(bits, 4, 1) == 1);
    g_assert(aztec_bits_get(bits, 5, 1) == 0);
    g_assert(aztec_bits_get(bits, 6, 2) == 3);
    g_assert(aztec_bits_get(bits, 8, 2) == 0);

    aztec_bits_free(bits);
}

/* Clear */

static
void
test_clear(
    void)
{
    AztecBits* bits = aztec_bits_new();

    aztec_bits_add(bits, 1, 1);
    g_assert(bits->count == 1);
    g_assert(aztec_bits_get(bits, 0, 1));

    /* Second clear does nothing */
    aztec_bits_clear(bits);
    aztec_bits_clear(bits);
    g_assert(!bits->count);
    g_assert(!aztec_bits_get(bits, 0, 1));

    aztec_bits_free(bits);
}

/* Set */

static
void
test_set(
    void)
{
    AztecBits* bits = aztec_bits_new();

    /* No bits are actually set but storage gets allocated and zeroed */
    aztec_bits_set(bits, 4, 0, 0);
    g_assert(bits->count == 4);
    g_assert(!aztec_bits_get(bits, 0, 8));

    /* This actually sets 2 bits */
    aztec_bits_set(bits, 4, 3, 2);
    g_assert(bits->count == 6);
    g_assert(aztec_bits_get(bits, 0, 8) == 0x30);

    /* This does nothing now */
    aztec_bits_set(bits, 4, 0, 0);
    g_assert(bits->count == 6);
    g_assert(aztec_bits_get(bits, 0, 8) == 0x30);

    /* Cross the unit boundary */
    aztec_bits_set(bits, 30, 0xffff, 33);
    g_assert(bits->count == 62);
    g_assert(aztec_bits_get(bits, 26, 8) == 0xf0);
    g_assert(aztec_bits_get(bits, 42, 8) == 0x0f);

    aztec_bits_free(bits);
}

/* Common */

#define TEST_(x) "/bits/" x

int main(int argc, char* argv[])
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func(TEST_("basic"), test_basic);
    g_test_add_func(TEST_("scatter"), test_scatter);
    g_test_add_func(TEST_("invert"), test_invert);
    g_test_add_func(TEST_("clear"), test_clear);
    g_test_add_func(TEST_("set"), test_set);
    return g_test_run();
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
