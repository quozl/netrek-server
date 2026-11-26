#define DNSBL_CHECK 1
#include "../../ntserv/ip.c"
#include <stdint.h>
#include <stddef.h>

static char g_cidr_dir[] = "/tmp/netrek_fuzz_cidr_XXXXXX";
static int g_init = 0;

static void init_cidr_dir(void)
{
    char p[256];
    const char *entries[] = {
        "192.168.16.0-20", "10.0.0.128-25", "172.16.0.0-12", "1.2.3.4-32",
        "127.0.0.1", "bad-abc", "1.2.3.4-99"
    };
    size_t i;

    if (mkdtemp(g_cidr_dir) == NULL)
        return;
    for (i = 0; i < sizeof(entries) / sizeof(entries[0]); i++) {
        FILE *f;
        snprintf(p, sizeof(p), "%s/%s", g_cidr_dir, entries[i]);
        f = fopen(p, "w");
        if (f) fclose(f);
    }
    g_init = 1;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    char raw[128];
    char dst[MAXHOSTNAMESIZE];
    char dnsbl[256];
    size_t cpy;

    if (!g_init)
        init_cidr_dir();

    memset(raw, 0, sizeof(raw));
    cpy = size < sizeof(raw) - 1 ? size : sizeof(raw) - 1;
    if (cpy > 0)
        memcpy(raw, data, cpy);
    raw[cpy] = '\0';

    copy_hostname(dst, raw, sizeof(dst));
    format_dnsbl_host(raw, XBLDOMAIN, dnsbl, sizeof(dnsbl));
    if (g_init)
        flag_test_cidr(g_cidr_dir, raw);

    return 0;
}
