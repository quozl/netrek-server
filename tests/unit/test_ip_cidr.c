#define DNSBL_CHECK 1
#include "../../ntserv/ip.c"
#include "test_harness.h"

static void touch_file(const char *path)
{
    FILE *f = fopen(path, "w");
    if (f) fclose(f);
}

static void test_cidr_matching(void)
{
    char dir[] = "/tmp/netrek_test_cidr_XXXXXX";
    char path[256];

    ASSERT_TRUE(mkdtemp(dir) != NULL);

    /* Non-byte-aligned /20 prefix (failed under old left-shift on little-endian) */
    snprintf(path, sizeof(path), "%s/192.168.16.0-20", dir);
    touch_file(path);
    ASSERT_EQ(flag_test_cidr(dir, "192.168.16.1"), 1);
    ASSERT_EQ(flag_test_cidr(dir, "192.168.31.254"), 1);
    ASSERT_EQ(flag_test_cidr(dir, "192.168.32.1"), 0);
    ASSERT_EQ(flag_test_cidr(dir, "192.168.15.255"), 0);
    unlink(path);

    /* Non-byte-aligned /25 prefix */
    snprintf(path, sizeof(path), "%s/10.0.0.128-25", dir);
    touch_file(path);
    ASSERT_EQ(flag_test_cidr(dir, "10.0.0.128"), 1);
    ASSERT_EQ(flag_test_cidr(dir, "10.0.0.255"), 1);
    ASSERT_EQ(flag_test_cidr(dir, "10.0.0.127"), 0);
    ASSERT_EQ(flag_test_cidr(dir, "10.0.1.128"), 0);
    unlink(path);

    /* /12 prefix */
    snprintf(path, sizeof(path), "%s/172.16.0.0-12", dir);
    touch_file(path);
    ASSERT_EQ(flag_test_cidr(dir, "172.16.0.1"), 1);
    ASSERT_EQ(flag_test_cidr(dir, "172.31.255.254"), 1);
    ASSERT_EQ(flag_test_cidr(dir, "172.32.0.1"), 0);
    unlink(path);

    /* /32 prefix */
    snprintf(path, sizeof(path), "%s/1.2.3.4-32", dir);
    touch_file(path);
    ASSERT_EQ(flag_test_cidr(dir, "1.2.3.4"), 1);
    ASSERT_EQ(flag_test_cidr(dir, "1.2.3.5"), 0);
    unlink(path);

    /* /0 prefix (must not invoke << 32 UB) */
    snprintf(path, sizeof(path), "%s/0.0.0.0-0", dir);
    touch_file(path);
    ASSERT_EQ(flag_test_cidr(dir, "203.0.113.42"), 1);
    unlink(path);

    /* Exact IP entry */
    snprintf(path, sizeof(path), "%s/198.51.100.7", dir);
    touch_file(path);
    ASSERT_EQ(flag_test_cidr(dir, "198.51.100.7"), 1);
    ASSERT_EQ(flag_test_cidr(dir, "198.51.100.8"), 0);
    unlink(path);

    /* Malformed mask entries must be ignored */
    snprintf(path, sizeof(path), "%s/1.2.3.4-abc", dir);
    touch_file(path);
    ASSERT_EQ(flag_test_cidr(dir, "1.2.3.4"), 0);
    unlink(path);

    snprintf(path, sizeof(path), "%s/1.2.3.4-33", dir);
    touch_file(path);
    ASSERT_EQ(flag_test_cidr(dir, "1.2.3.4"), 0);
    unlink(path);

    /* NULL / invalid IP input must return 0 safely */
    ASSERT_EQ(flag_test_cidr(dir, NULL), 0);
    ASSERT_EQ(flag_test_cidr(dir, "not.an.ip"), 0);

    rmdir(dir);
}

static void test_copy_hostname_sanitisation(void)
{
    char dst[16];

    copy_hostname(dst, "valid-host_1.example:80", sizeof(dst));
    ASSERT_STREQ(dst, "valid-host_1.ex");

    copy_hostname(dst, "a;rm -rf /\n`x`", sizeof(dst));
    ASSERT_STREQ(dst, "a_rm_-rf____x_");

    dst[0] = 'X';
    copy_hostname(dst, "abc", 1);
    ASSERT_STREQ(dst, "");

    /* Zero-size buffer must not write */
    dst[0] = 'Z';
    copy_hostname(dst, "abc", 0);
    ASSERT_EQ(dst[0], 'Z');
}

static void test_dnsbl_host_formatting(void)
{
    char dnshost[256];

    ASSERT_EQ(format_dnsbl_host("1.2.3.4", XBLDOMAIN, dnshost, sizeof(dnshost)), 1);
    ASSERT_STREQ(dnshost, "4.3.2.1." XBLDOMAIN);

    ASSERT_EQ(format_dnsbl_host("not.an.ip", XBLDOMAIN, dnshost, sizeof(dnshost)), 0);
    ASSERT_EQ(format_dnsbl_host("1.2.3.4.5", XBLDOMAIN, dnshost, sizeof(dnshost)), 0);
}

static void test_ip_ignore_update_missing_dir(void)
{
    /* Should not crash when fopen() fails on a non-existent directory */
    ip_ignore_ip_update("127.0.0.1", "127.0.0.2", MALL);
    ASSERT_TRUE(1);
}

int main(void)
{
    RUN_TEST(test_cidr_matching);
    RUN_TEST(test_copy_hostname_sanitisation);
    RUN_TEST(test_dnsbl_host_formatting);
    RUN_TEST(test_ip_ignore_update_missing_dir);
    TEST_SUMMARY("test_ip_cidr");
}
