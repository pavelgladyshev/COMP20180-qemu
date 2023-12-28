/* Practice device
 *
 */

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "hw/misc/practice.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qapi/error.h"

static uint64_t practice_read(void *opaque, hwaddr offset, unsigned size)
{
    PracticeDeviceState *s = PRACTICE_DEVICE(opaque);

    qemu_log_mask(LOG_UNIMP, "%s: practicelemented device read  "
                  "(size %d, offset 0x%0*" HWADDR_PRIx ")\n",
                  s->name, size, s->offset_fmt_width, offset);
    return 0;
}

static void practice_write(void *opaque, hwaddr offset,
                        uint64_t value, unsigned size)
{
    PracticeDeviceState *s = PRACTICE_DEVICE(opaque);

    qemu_log_mask(LOG_UNIMP, "%s: practice device write "
                  "(size %d, offset 0x%0*" HWADDR_PRIx
                  ", value 0x%0*" PRIx64 ")\n",
                  s->name, size, s->offset_fmt_width, offset, size << 1, value);
}

static const MemoryRegionOps practice_ops = {
    .read = practice_read,
    .write = practice_write,
    .impl.min_access_size = 1,
    .impl.max_access_size = 8,
    .valid.min_access_size = 1,
    .valid.max_access_size = 8,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void practice_realize(DeviceState *dev, Error **errp)
{
    PracticeDeviceState *s = PRACTICE_DEVICE(dev);

    if (s->size == 0) {
        error_setg(errp, "property 'size' not specified or zero");
        return;
    }

    if (s->name == NULL) {
        error_setg(errp, "property 'name' not specified");
        return;
    }

    s->offset_fmt_width = DIV_ROUND_UP(64 - clz64(s->size - 1), 4);

    memory_region_init_io(&s->iomem, OBJECT(s), &practice_ops, s,
                          s->name, s->size);
    sysbus_init_mmio(SYS_BUS_DEVICE(s), &s->iomem);
}

static Property practice_properties[] = {
    DEFINE_PROP_UINT64("size", PracticeDeviceState, size, 0),
    DEFINE_PROP_STRING("name", PracticeDeviceState, name),
    DEFINE_PROP_END_OF_LIST(),
};

static void practice_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = practice_realize;
    device_class_set_props(dc, practice_properties);
}

static const TypeInfo practice_info = {
    .name = TYPE_PRACTICE_DEVICE,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(PracticeDeviceState),
    .class_init = practice_class_init,
};

static void practice_register_types(void)
{
    type_register_static(&practice_info);
}

type_init(practice_register_types)
