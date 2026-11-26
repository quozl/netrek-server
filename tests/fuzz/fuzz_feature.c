#include "config.h"
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "packets.h"
#include "proto.h"

static int g_init = 0;

static void init_features_once(void)
{
    char tmpl[] = "/tmp/netrek_fuzz_feat_XXXXXX";
    int fd = mkstemp(tmpl);
    if (fd >= 0) {
        FILE *f = fdopen(fd, "w");
        if (f) {
            fprintf(f,
                    "FPS C 50\n"
                    "UPS C 10\n"
                    "SHOW_ALL_TRACTORS S ON\n"
                    "SHIP_CAP S ON\n"
                    "NEWMACRO C ON\n"
                    "SMARTMACRO C ON\n");
            fclose(f);
            strncpy(Feature_File, tmpl, sizeof(Feature_File) - 1);
            Feature_File[sizeof(Feature_File) - 1] = '\0';
            readFeatures();
        } else {
            close(fd);
        }
        unlink(tmpl);
    }
    g_init = 1;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    struct feature_cpacket cp;
    struct feature_spacket sp;
    size_t cpy;

    if (!g_init)
        init_features_once();

    memset(&cp, 0, sizeof(cp));
    memset(&sp, 0, sizeof(sp));
    cpy = size < sizeof(cp) ? size : sizeof(cp);
    if (cpy > 0)
        memcpy(&cp, data, cpy);
    cp.type = CP_FEATURE;

    getFeature(&cp, &sp);
    return 0;
}
