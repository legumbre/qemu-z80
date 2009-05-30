#include "hw/hw.h"
#include "hw/boards.h"

void register_machines(void)
{
    qemu_register_machine(&zxspec_machine);
}

void cpu_save(QEMUFile *f, void *opaque)
{
}

int cpu_load(QEMUFile *f, void *opaque, int version_id)
{
    return 0;
}
