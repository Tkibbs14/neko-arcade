/* nlm_fill (C, the device) against c_fill (tools/nlm_quality.py, the panel's copy): reads "key<TAB>value<TAB>line"
 * and prints each filled line; tools/filltest.sh compares the two. */
#include <stdio.h>
#include <string.h>
#include "../src/nekolm.h"

int str_len(const char *s) { return (int)strlen(s); }
u32 time_us(void) { return 0; }
u32 rnd(void) { return 0; }

int main(void)
{
    char buf[512], out[160];
    while (fgets(buf, sizeof buf, stdin)) {
        buf[strcspn(buf, "\r\n")] = 0;
        char *key = buf, *value = strchr(key, 9);
        if (!value) continue;
        *value++ = 0;
        char *line = strchr(value, 9);
        if (!line) continue;
        *line++ = 0;
        nlm_fill(out, line, key, value);
        puts(out);
    }
    return 0;
}
