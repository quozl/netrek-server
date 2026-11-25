#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "packets.h"
#include "proto.h"
#include "test_harness.h"

/* Minimal stubs for symbols referenced by warning.c / feature.c */
static char g_last_warning[MSG_LEN];

void sendClientPacket(void *packet)
{
    struct warning_spacket *w = (struct warning_spacket *) packet;
    if (w && w->type == SP_WARNING) {
        strncpy(g_last_warning, w->mesg, sizeof(g_last_warning) - 1);
        g_last_warning[sizeof(g_last_warning) - 1] = '\0';
    }
}

void pmessage2(int recip, int group, char *address, u_char from,
               const char *fmt, ...)
{
    (void) recip; (void) group; (void) address; (void) from; (void) fmt;
}

int sizes[TOTAL_SPACKETS];

static void test_feature_parser_and_lookup(void)
{
    char tmpl[] = "/tmp/netrek_test_feat_XXXXXX";
    int fd;
    FILE *f;
    struct feature_cpacket cp;
    struct feature_spacket sp;
    char longname[200];

    fd = mkstemp(tmpl);
    ASSERT_TRUE(fd >= 0);
    f = fdopen(fd, "w");
    ASSERT_TRUE(f != NULL);

    /* Oversized feature name, oversized value, malformed lines, and valid entries */
    memset(longname, 'X', sizeof(longname) - 1);
    longname[sizeof(longname) - 1] = '\0';
    fprintf(f, "# comment\n");
    fprintf(f, "\n");
    fprintf(f, "OLD_STYLE: ON\n");
    fprintf(f, "MALFORMED_ONE_TOKEN\n");
    fprintf(f, "%s S %s 1 2\n", longname, longname);
    fprintf(f, "SHOW_ALL_TRACTORS S ON\n");
    fprintf(f, "FPS C 50\n");
    fclose(f);

    strncpy(Feature_File, tmpl, sizeof(Feature_File) - 1);
    Feature_File[sizeof(Feature_File) - 1] = '\0';
    readFeatures();

    /* Non-NUL-terminated 80-byte feature name from wire must not overflow */
    memset(&cp, 0, sizeof(cp));
    memset(cp.name, 'A', sizeof(cp.name));
    memset(&sp, 0, sizeof(sp));
    getFeature(&cp, &sp);
    ASSERT_EQ(sp.name[sizeof(sp.name) - 1], '\0');
    ASSERT_EQ(sp.value, -1);

    /* High-bit bytes in feature name must not trigger signed-char UB */
    memset(&cp, 0, sizeof(cp));
    cp.name[0] = (char) 0xE9;
    cp.name[1] = (char) 0xFF;
    cp.name[2] = '\0';
    getFeature(&cp, &sp);
    ASSERT_EQ(sp.value, -1);

    /* Case-insensitive match works */
    memset(&cp, 0, sizeof(cp));
    strcpy(cp.name, "fps");
    getFeature(&cp, &sp);
    ASSERT_EQ(sp.feature_type, 'C');
    ASSERT_EQ(sp.value, 50);

    unlink(tmpl);
}

static void test_bans_check_permanent_overflow_and_hits_reset(void)
{
    char tmpl[] = "/tmp/netrek_test_bans_XXXXXX";
    int fd;
    FILE *f;
    char longlogin[100];
    char longhost[100];

    fd = mkstemp(tmpl);
    ASSERT_TRUE(fd >= 0);
    f = fdopen(fd, "w");
    ASSERT_TRUE(f != NULL);

    memset(longlogin, 'u', sizeof(longlogin) - 1);
    longlogin[sizeof(longlogin) - 1] = '\0';
    memset(longhost, 'h', sizeof(longhost) - 1);
    longhost[sizeof(longhost) - 1] = '\0';

    /* Line 1: oversized login and host */
    fprintf(f, "%s@%s\n", longlogin, longhost);
    /* Line 2: matches login 'alice' ONLY (wrong host) */
    fprintf(f, "alice@evil.example\n");
    /* Line 3: matches host 'good.example' ONLY (wrong login) */
    fprintf(f, "bob@good.example\n");
    /* Line 4: exact match for mallory@bad.example */
    fprintf(f, "mallory@bad.example\n");
    fclose(f);

    strncpy(Banned_File, tmpl, sizeof(Banned_File) - 1);
    Banned_File[sizeof(Banned_File) - 1] = '\0';

    /* alice@good.example must NOT be banned (hits must reset between lines 2 and 3) */
    ASSERT_EQ(bans_check_permanent("alice", "good.example"), FALSE);
    /* mallory@bad.example MUST be banned */
    ASSERT_EQ(bans_check_permanent("mallory", "bad.example"), TRUE);

    unlink(tmpl);
}

static void test_new_warning_long_message_no_overflow(void)
{
    char big[300];

    memset(big, 'W', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';
    eventlog = 0;
    send_short = 0;
    g_last_warning[0] = '\0';

    new_warning(UNDEF, "prefix:%s", big);
    ASSERT_TRUE(strncmp(g_last_warning, "prefix:WWWW", 11) == 0);
    ASSERT_EQ(g_last_warning[MSG_LEN - 1], '\0');
}

int main(void)
{
    RUN_TEST(test_feature_parser_and_lookup);
    RUN_TEST(test_bans_check_permanent_overflow_and_hits_reset);
    RUN_TEST(test_new_warning_long_message_no_overflow);
    TEST_SUMMARY("test_parsers");
}
