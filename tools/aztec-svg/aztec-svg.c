/*
 * Copyright (C) 2022 by Slava Monich <slava@monich.com>
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

#include <errno.h>
#include <stdio.h>
#include <string.h>

#define RET_OK 0
#define RET_ERR 1
#define RET_CMDLINE 2

#define SVG_UNITS(u) \
    u(EM,em) u(EX,ex) u(PX,px) u(PT,pt) u(PC,pc) u(CM,cm) u(MM,mm) u(IN,in)

typedef enum svg_unit {
    #define SVG_UNIT_(U,u) SVG_UNIT_##U,
    SVG_UNITS(SVG_UNIT_)
    #undef SVG_UNIT_
    SVG_UNIT_COUNT
} SVG_UNIT;

#define DEFAULT_UNIT_NAME "px"
#define DEFAULT_UNIT SVG_UNIT_PX
static const char* svg_units[SVG_UNIT_COUNT] = {
    #define SVG_UNIT_(U,u) #u,
    SVG_UNITS(SVG_UNIT_)
    #undef SVG_UNIT_
};

typedef struct svg_size {
    double size;
    SVG_UNIT unit;
} SvgSize;

typedef struct app_options {
    SvgSize pixel;
    int border;
} AppOptions;

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
options_init(
    AppOptions* opts)
{
    memset(opts, 0, sizeof(*opts));
    opts->pixel.size = 1;
    opts->pixel.unit = DEFAULT_UNIT;
    opts->border = 1;
}

static
int
save_symbol(
    const AztecSymbol* sym,
    const AppOptions* opts,
    FILE* out)
{
    const double size = (sym->size + 2 * opts->border) * opts->pixel.size;
    const char* unit = (opts->pixel.unit == SVG_UNIT_PX) ? "" :
        svg_units[opts->pixel.unit];
    const char* bgcolor = "#ffffff";
    const char* fgcolor = "#000000";
    const char* indent = "  ";
    gboolean** rows = g_new(gboolean*, sym->size);
    gboolean* pixels = g_new(gboolean, sym->size * sym->size);
    guint i, j;

    /* Unpack the pixels for convenience */
    for (i = 0; i < sym->size; i++) {
        const guint8* ptr = sym->rows[i];
        gboolean* row = pixels + i * sym->size;
        guint k = 0;

        rows[i] = row;
        while (k < sym->size) {
            guint byte = *ptr++;

            for (j = 0; j < 8 && k < sym->size; k++, j++, byte >>= 1) {
                row[k] = (byte & 1);
            }
        }
    }

    fputs("<?xml version=\"1.0\" standalone=\"no\"?>\n", out);
    fprintf(out, "<svg version=\"1.1\" "
        "width=\"%g%s\" height=\"%g%s\" "
        "viewBox=\"0 0 %g %g\" "
        "xmlns=\"http://www.w3.org/2000/svg\">\n",
         size, unit, size, unit, size, size);

    /* Background */
    fprintf(out, "%s<rect x=\"0\" y=\"0\" "
        "width=\"%g\" height=\"%g\" "
        "style=\"fill:%s;fill-opacity:1\"/>\n",
        indent, size, size, bgcolor);

    /* Symbol */
    fprintf(out, "%s<g style=\"fill:%s;fill-opacity:1\">\n", indent, fgcolor);
    for (i = 0; i < sym->size; i++) {
        const gboolean* row = rows[i];
        const double y = opts->pixel.size * (i + opts->border);

        for (j = 0; j < sym->size; j++) {
            const double x = opts->pixel.size * (j + opts->border);

            if (row[j]) {
                fprintf(out, "%s%s<rect x=\"%g\" y=\"%g\" "
                    "width=\"%g\" height=\"%g\"/>\n",
                    indent, indent, x, y,
                    opts->pixel.size, opts->pixel.size);
            }
        }
    }

    g_free(rows);
    g_free(pixels);
    fprintf(out, "%s</g>\n</svg>\n", indent);
    return ferror(out) ? RET_ERR : RET_OK;
}

static
gboolean
parse_svg_size(
    const char* in,
    SvgSize* out,
    GError** error)
{
    char* str = g_strstrip(g_strdup(in));
    char* end = NULL;
    SVG_UNIT u, unit = DEFAULT_UNIT;
    gsize len = strlen(str);
    double value;
    gboolean ok;

    /* Parse the units */
    for (u = 0; u < SVG_UNIT_COUNT; u++) {
        const char* uname = svg_units[u];
        const gsize ulen = strlen(uname);

        if (len >= ulen && !g_ascii_strcasecmp(str + (len - ulen), uname)) {
            str[len - ulen] = 0;
            g_strchomp(str);
            unit = u;
            break;
        }
    }

    /* And the number */
    value = strtod(str, &end);
    if (end && !end[0] && value > 0) {
        out->size = value;
        out->unit = unit;
        ok = TRUE;
    } else {
        if (error) {
            g_propagate_error(error, g_error_new(G_OPTION_ERROR,
               G_OPTION_ERROR_BAD_VALUE, "Invalid %s \'%s\'",
               (u == SVG_UNIT_COUNT) ? "size" : "number", str));
        }
        ok = FALSE;
    }

    g_free(str);
    return ok;
}

static
gboolean
option_pixel_size(
    const gchar* name,
    const gchar* value,
    gpointer data,
    GError** error)
{
    AppOptions* opts = data;

    return parse_svg_size(value, &opts->pixel, error);
}

int
main(
    int argc,
    char* argv[])
{
    int ret = RET_ERR;
    gboolean ok;
    const char* file = NULL;
    int errcorr = AZTEC_CORRECTION_DEFAULT;
    GError* error = NULL;
    AppOptions opts;
    GOptionContext* parser;
    GOptionGroup* group;
    GOptionEntry entries[] = {
        { "pixel", 'p', 0, G_OPTION_ARG_CALLBACK, &option_pixel_size,
          "Pixel size [1" DEFAULT_UNIT_NAME "]", "SIZE" },
        { "correction", 'c', 0, G_OPTION_ARG_INT, &errcorr,
          "Error correction [23]", "PERCENT" },
        { "border", 'b', 0, G_OPTION_ARG_INT, &opts.border,
          "Border around the symbol [1]", "PIXELS" },
        { "file", 'f', 0, G_OPTION_ARG_FILENAME, &file,
          "Encode data from FILE", "FILE" },
        { NULL }
    };

    options_init(&opts);
    parser = g_option_context_new("[TEXT] SVG");
    group = g_option_group_new("", "", "", &opts, NULL);
    g_option_group_add_entries(group, entries);
    g_option_context_set_main_group(parser, group);
    g_option_context_set_summary(parser,
        "Generates Aztec symbol as an SVG file.");

    ok = g_option_context_parse(parser, &argc, &argv, &error);
    if (ok && opts.border >= 0 &&
       ((argc == 3 && !file) || (argc == 2 && file))) {
        const char* output = argv[argc - 1];
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
                    const void* contents = g_mapped_file_get_contents(map);
                    const gsize length = g_mapped_file_get_length(map);

                    bytes = g_bytes_new_with_free_func(contents, length,
                        (GDestroyNotify)g_mapped_file_unref, map);
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

                    if (!strcmp(output, "-")) {
                        out = stdout;
                    } else {
                        out = f = fopen(output, "wb");
                        if (!out) {
                            errmsg("%s: %s\n", output, strerror(errno));
                        }
                    }

                    if (out) {
                        ret = save_symbol(symbol, &opts, out);
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
            char* help = g_option_context_get_help(parser, TRUE, NULL);
            errmsg("%s", help);
            g_free(help);
        }
        ret = RET_CMDLINE;
    }
    g_option_context_free(parser);
    return ret;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
