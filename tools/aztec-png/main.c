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

#include "aztec_encode.h"

#include <png.h>
#include <errno.h>
#include <setjmp.h>

#define RET_OK 0
#define RET_ERR 1
#define RET_CMDLINE 2

static
void
errmsg(
    const char* format,
    ...) G_GNUC_PRINTF(1,2);

static
void
errmsg(
    const char* format,
    ...)
{
    va_list va;
    va_start(va, format);
    vfprintf(stderr, format, va);
    va_end(va);
}

static
void
save_error(
    png_structp png,
    png_const_charp msg)
{
    jmp_buf* jmp = png_get_error_ptr(png);

    errmsg("%s", msg);
    longjmp(*jmp, 1);
}

static
int
save_symbol(
    AztecSymbol* symbol,
    int scale,
    int border,
    FILE* out)
{
    int ret = RET_ERR;
    const int bs = border * scale;
    const int n = symbol->size * scale;
    const int n2 = n + 2 * bs;
    const int rowsize = (n2 + 7) / 8;
    jmp_buf jmp;
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, &jmp,
        save_error, NULL);

    if (png) {
        png_infop info = png_create_info_struct(png);

        if (info) {
            guint8* row = g_malloc(rowsize);

            if (!setjmp(jmp)) {
                guint i;

                png_init_io(png, out);
                png_set_filter(png, 0, PNG_FILTER_NONE);
                png_set_IHDR(png, info, n2, n2, 1, PNG_COLOR_TYPE_GRAY,
                    PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                    PNG_FILTER_TYPE_DEFAULT);
                png_set_invert_mono(png); /* 1 is black */
                png_write_info(png, info);

                /* Top border */
                memset(row, 0, rowsize);
                for (i = 0; i < bs; i++) {
                    png_write_row(png, (png_bytep)row);
                }

                /*
                 * Transform the rows (scale and add borders) and at the
                 * same time swap the bit order. In PNG most significant
                 * bit codes the left-most pixel.
                 */
                for (i = 0; i < symbol->size; i++) {
                    guint8* dest = row;
                    guint8* end = dest + rowsize;
                    const guint8* ptr = symbol->rows[i];
                    int k, j, d = bs % 8;

                    memset(row, 0, rowsize);
                    dest += bs / 8;
                    for (k = 0; k < symbol->size; k += 8) {
                        guint8 byte = *ptr++;
                        int b;

                        for (b = 0; b < 8 && dest < end; b++, byte >>= 1) {
                            const int bit = (byte & 1);

                            for (j = 0; j < scale && dest < end; j++) {
                                *dest <<= 1;
                                *dest |= bit;
                                if ((d++) == 7) {
                                    d = 0;
                                    dest++;
                                }
                            }
                        }
                    }
                    for (j = 0; j < scale; j++) {
                        png_write_row(png, (png_bytep)row);
                    }
                }

                /* Bottom border */
                memset(row, 0, rowsize);
                for (i = 0; i < bs; i++) {
                    png_write_row(png, (png_bytep)row);
                }
                png_write_end(png, info);
                ret = RET_OK;
            }
            png_destroy_write_struct(&png, &info);
            g_free(row);
        }
    }
    return ret;
}

int
main(
    int argc,
    char* argv[])
{
    int ret = RET_ERR;
    int scale = 1;
    int errcorr = AZTEC_CORRECTION_DEFAULT;
    int border = 1;
    const char* file = NULL;
    gboolean ok;
    GError* error = NULL;
    GOptionContext* options;
    GOptionEntry entries[] = {
        { "scale", 's', 0, G_OPTION_ARG_INT, &scale,
          "Scale factor [1]", "SCALE" },
        { "correction", 'c', 0, G_OPTION_ARG_INT, &errcorr,
          "Error correction [23]", "PERCENT" },
        { "border", 'b', 0, G_OPTION_ARG_INT, &border,
          "Border around the symbol [1]", "PIXELS" },
        { "file", 'f', 0, G_OPTION_ARG_FILENAME, &file,
          "Encode data from FILE", "FILE" },
        { NULL }
    };

    /* g_type_init has been deprecated since version 2.36
     * the type system is initialised automagically since then */
#if !GLIB_CHECK_VERSION(2, 36, 0)
    g_type_init();
#endif

    options = g_option_context_new("[TEXT] PNG");
    g_option_context_add_main_entries(options, entries, NULL);
    g_option_context_set_summary(options,
        "Generates Aztec symbol as a PNG file.");
    ok = g_option_context_parse(options, &argc, &argv, &error);

    if (ok && scale > 0 && border >= 0 &&
        ((argc == 3 && !file) || (argc == 2 && file))) {
        const char* png = argv[argc - 1];
        const void* data = NULL;
        gsize size;
        GBytes* bytes = NULL;

        if (file) {
            if (!strcmp(file, "-")) {
                GByteArray* buf = g_byte_array_new();
                int c;

                /* Read stdin until EOF */
                while ((c = getchar()) != EOF) {
                    const guint8 byte = (guint8)c;

                    g_byte_array_append(buf, &byte, 1);
                }
                bytes = g_byte_array_free_to_bytes(buf);
            } else {
                GMappedFile* map = g_mapped_file_new(file, FALSE, &error);
                if (map) {
                    bytes = g_mapped_file_get_bytes(map);
                    g_mapped_file_unref(map);
                } else {
                    errmsg("%s\n", error->message);
                    g_error_free(error);
                }
            }
            if (bytes) {
                data = g_bytes_get_data(bytes, &size);
            }
        } else {
            data = argv[1];
            size = strlen(data);
        }

        if (data) {
            if (size) {
                AztecSymbol* symbol = aztec_encode(data, size, errcorr);

                if (symbol) {
                    FILE* f = NULL;
                    FILE* out = NULL;

                    if (!strcmp(png, "-")) {
                        out = stdout;
                    } else {
                        out = f = fopen(png, "wb");
                        if (!out) {
                            errmsg("%s: %s\n", png, strerror(errno));
                        }
                    }

                    if (out) {
                        ret = save_symbol(symbol, scale, border, out);
                        if (f) {
                            fclose(f);
                        }
                    }
                    aztec_symbol_free(symbol);
                } else {
                    errmsg("Failed to generate symbol (too much data?)\n");
                }
            } else {
                errmsg("Nothing to encode.\n");
            }
        }
        if (bytes) {
            g_bytes_unref(bytes);
        }
    } else {
        if (error) {
            errmsg("%s\n", error->message);
            g_error_free(error);
        } else {
            char* help = g_option_context_get_help(options, TRUE, NULL);
            errmsg("%s", help);
            g_free(help);
        }
        ret = RET_CMDLINE;
    }
    g_option_context_free(options);
    return ret;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
