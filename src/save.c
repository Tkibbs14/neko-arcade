#include "save.h"

Save save;

void save_check(void)
{
    if (save.magic == SAVE_MAGIC && save.version == SAVE_VERSION)
        return;
    mem_set(&save, 0, sizeof(save));
    save.magic = SAVE_MAGIC;
    save.version = SAVE_VERSION;
    for (int i = 0; i < 8; i++)
        save.hearts[i] = 10;
}
