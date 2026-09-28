#include "lines.h"

const char *line_pick(int spk, int sit)
{
    for (int i = 0; i < line_pool_count; i++)
        if (line_pools[i].spk == spk && line_pools[i].sit == sit && line_pools[i].count)
            return line_pools[i].lines[rnd() % (u32)line_pools[i].count];
    return 0;
}
