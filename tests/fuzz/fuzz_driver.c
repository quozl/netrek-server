#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

extern int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static int run_file(const char *path)
{
    FILE *f;
    long sz;
    uint8_t *buf;
    size_t n;

    f = fopen(path, "rb");
    if (!f) {
        perror(path);
        return 1;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return 1;
    }
    sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return 1;
    }
    rewind(f);

    buf = (uint8_t *) malloc((size_t) sz + 1);
    if (!buf) {
        fclose(f);
        return 1;
    }
    n = fread(buf, 1, (size_t) sz, f);
    fclose(f);
    buf[n] = 0;

    LLVMFuzzerTestOneInput(buf, n);
    free(buf);
    return 0;
}

int main(int argc, char **argv)
{
    int i;
    if (argc < 2) {
        fprintf(stderr, "usage: %s <corpus-file>...\n", argv[0]);
        return 2;
    }
    for (i = 1; i < argc; i++) {
        if (run_file(argv[i]) != 0)
            return 1;
    }
    printf("Executed %d corpus input(s) cleanly.\n", argc - 1);
    return 0;
}
