/*
 * Emulation of some of the "tools" from RARS RISC-V Simulator
 * Based on Jazz_led by Herve Poussineau
 * 
 * (C) Pavel Gladyshev, 2024
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include "qemu/module.h"
#include "ui/console.h"
#include "ui/pixel_ops.h"
#include "trace.h"
#include "hw/sysbus.h"
#include "migration/vmstate.h"
#include "qom/object.h"

typedef enum {
    REDRAW_NONE = 0, REDRAW_DISPLAYS = 1, REDRAW_BACKGROUND = 2,
} screen_state_t;

#define TYPE_RARS_TOOLS "rars_tools"
OBJECT_DECLARE_SIMPLE_TYPE(RarsToolsState, RARS_TOOLS)

struct RarsToolsState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    uint16_t segments_left;
    uint16_t segments_right;
    QemuConsole *con;
    screen_state_t state;
};

static uint64_t rars_tools_read(void *opaque, hwaddr addr,
                              unsigned int size)
{
    RarsToolsState *s = opaque;
    uint8_t val;

    switch(addr) {
        case 0x10: 
            val = s->segments_right;
            if (size>1) {
                val |= s->segments_left << 8;
            }
            break;
        case 0x11: 
            val = s->segments_left;
            break;
        default:
            val = 0;
    }
    
    trace_rars_tools_read(addr, val);
    return val;
}

static void rars_tools_write(void *opaque, hwaddr addr,
                           uint64_t val, unsigned int size)
{
    RarsToolsState *s = opaque;
    uint8_t new_val = val & 0xff;

    trace_rars_tools_write(addr, new_val);

    switch (addr) {
        case 0x10: 
            s->segments_right = val & 0xff;
            if (size > 1) 
            {
                val >>= 8;
                s->segments_left = val & 0xff;
            }
            break;
        case 0x11:
            s->segments_left = val & 0xff;
            break;
    }
    s->state |= REDRAW_DISPLAYS;
}

static const MemoryRegionOps rars_tools_mem_ops = {
    .read = rars_tools_read,
    .write = rars_tools_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl.min_access_size = 1,
    .impl.max_access_size = 4,
};

/***********************************************************/
/* rars_tools display */

static void draw_horizontal_line(DisplaySurface *ds,
                                 int posy, int posx1, int posx2,
                                 uint32_t color)
{
    uint8_t *d;
    int x, bpp;

    bpp = (surface_bits_per_pixel(ds) + 7) >> 3;
    d = surface_data(ds) + surface_stride(ds) * posy + bpp * posx1;
    switch (bpp) {
    case 1:
        for (x = posx1; x <= posx2; x++) {
            *((uint8_t *)d) = color;
            d++;
        }
        break;
    case 2:
        for (x = posx1; x <= posx2; x++) {
            *((uint16_t *)d) = color;
            d += 2;
        }
        break;
    case 4:
        for (x = posx1; x <= posx2; x++) {
            *((uint32_t *)d) = color;
            d += 4;
        }
        break;
    }
}

static void draw_vertical_line(DisplaySurface *ds,
                               int posx, int posy1, int posy2,
                               uint32_t color)
{
    uint8_t *d;
    int y, bpp;

    bpp = (surface_bits_per_pixel(ds) + 7) >> 3;
    d = surface_data(ds) + surface_stride(ds) * posy1 + bpp * posx;
    switch (bpp) {
    case 1:
        for (y = posy1; y <= posy2; y++) {
            *((uint8_t *)d) = color;
            d += surface_stride(ds);
        }
        break;
    case 2:
        for (y = posy1; y <= posy2; y++) {
            *((uint16_t *)d) = color;
            d += surface_stride(ds);
        }
        break;
    case 4:
        for (y = posy1; y <= posy2; y++) {
            *((uint32_t *)d) = color;
            d += surface_stride(ds);
        }
        break;
    }
}

static void rars_tools_draw_7segment_display(DisplaySurface *surface, uint32_t color_segment, uint32_t color_led, uint8_t segments, int x, int y)
{
            /* display segments */
        draw_horizontal_line(surface, y+40, x+10, x+40,
                             (segments & 0x40) ? color_segment : 0);
        draw_vertical_line(surface, x+10, y+10, y+40,
                           (segments & 0x20) ? color_segment : 0);
        draw_vertical_line(surface, x+10, y+40, y+70,
                           (segments & 0x10) ? color_segment : 0);
        draw_horizontal_line(surface, y+70, x+10, x+40,
                             (segments & 0x8) ? color_segment : 0);
        draw_vertical_line(surface, x+40, y+40, y+70,
                           (segments & 0x4) ? color_segment : 0);
        draw_vertical_line(surface, x+40, y+10, y+40,
                           (segments & 0x2) ? color_segment : 0);
        draw_horizontal_line(surface, y+10, x+10, x+40,
                             (segments & 0x01) ? color_segment : 0);

        /* display led */
        if (!(segments & 0x80)) {
            color_led = 0; /* black */
        }
        draw_horizontal_line(surface, y+68, x+50, x+50, color_led);
        draw_horizontal_line(surface, y+69, x+49, x+51, color_led);
        draw_horizontal_line(surface, y+70, x+48, x+52, color_led);
        draw_horizontal_line(surface, y+71, x+49, x+51, color_led);
        draw_horizontal_line(surface, y+72, x+50, x+50, color_led);
}

static void rars_tools_update_display(void *opaque)
{
    RarsToolsState *s = opaque;
    DisplaySurface *surface = qemu_console_surface(s->con);
    uint8_t *d1;
    uint32_t color_segment, color_led;
    int y, bpp;

    if (s->state & REDRAW_BACKGROUND) {
        /* clear screen */
        bpp = (surface_bits_per_pixel(surface) + 7) >> 3;
        d1 = surface_data(surface);
        for (y = 0; y < surface_height(surface); y++) {
            memset(d1, 0x00, surface_width(surface) * bpp);
            d1 += surface_stride(surface);
        }
    }

    if (s->state & REDRAW_DISPLAYS) {
        /* set colors according to bpp */
        switch (surface_bits_per_pixel(surface)) {
        case 8:
            color_segment = rgb_to_pixel8(0xaa, 0xaa, 0xaa);
            color_led = rgb_to_pixel8(0x00, 0xff, 0x00);
            break;
        case 15:
            color_segment = rgb_to_pixel15(0xaa, 0xaa, 0xaa);
            color_led = rgb_to_pixel15(0x00, 0xff, 0x00);
            break;
        case 16:
            color_segment = rgb_to_pixel16(0xaa, 0xaa, 0xaa);
            color_led = rgb_to_pixel16(0x00, 0xff, 0x00);
            break;
        case 24:
            color_segment = rgb_to_pixel24(0xaa, 0xaa, 0xaa);
            color_led = rgb_to_pixel24(0x00, 0xff, 0x00);
            break;
        case 32:
            color_segment = rgb_to_pixel32(0xaa, 0xaa, 0xaa);
            color_led = rgb_to_pixel32(0x00, 0xff, 0x00);
            break;
        default:
            return;
        }

        rars_tools_draw_7segment_display(surface, color_segment, color_led, s->segments_left, 0, 0);
        rars_tools_draw_7segment_display(surface, color_segment, color_led, s->segments_right, 60, 0);
    }

    s->state = REDRAW_NONE;
    dpy_gfx_update_full(s->con);
}

static void rars_tools_invalidate_display(void *opaque)
{
    RarsToolsState *s = opaque;
    s->state |= REDRAW_DISPLAYS | REDRAW_BACKGROUND;
}

static void rars_tools_text_update(void *opaque, console_ch_t *chardata)
{
    RarsToolsState *s = opaque;
    char buf[5];

    dpy_text_cursor(s->con, -1, -1);
    qemu_console_resize(s->con, 2, 1);

    /* TODO: draw the segments */
    snprintf(buf, 5, "%02hhx%02hhx", s->segments_left,s->segments_right);
    console_write_ch(chardata++, ATTR2CHTYPE(buf[0], QEMU_COLOR_BLUE,
                                             QEMU_COLOR_BLACK, 1));
    console_write_ch(chardata++, ATTR2CHTYPE(buf[1], QEMU_COLOR_BLUE,
                                             QEMU_COLOR_BLACK, 1));
    console_write_ch(chardata++, ATTR2CHTYPE(buf[2], QEMU_COLOR_GREEN,
                                             QEMU_COLOR_BLACK, 1));
    console_write_ch(chardata++, ATTR2CHTYPE(buf[3], QEMU_COLOR_GREEN,
                                             QEMU_COLOR_BLACK, 1));

    dpy_text_update(s->con, 0, 0, 4, 1);
}

static int rars_tools_post_load(void *opaque, int version_id)
{
    /* force refresh */
    rars_tools_invalidate_display(opaque);

    return 0;
}

static const VMStateDescription vmstate_rars_tools = {
    .name = "rars_tools",
    .version_id = 0,
    .minimum_version_id = 0,
    .post_load = rars_tools_post_load,
    .fields = (VMStateField[]) {
        VMSTATE_UINT16(segments_left, RarsToolsState),
        VMSTATE_UINT16(segments_right, RarsToolsState),
        VMSTATE_END_OF_LIST()
    }
};

static const GraphicHwOps rars_tools_graphic_ops = {
    .invalidate  = rars_tools_invalidate_display,
    .gfx_update  = rars_tools_update_display,
    .text_update = rars_tools_text_update,
};

static void rars_tools_init(Object *obj)
{
    RarsToolsState *s = RARS_TOOLS(obj);
    SysBusDevice *dev = SYS_BUS_DEVICE(obj);

    memory_region_init_io(&s->iomem, obj, &rars_tools_mem_ops, s, "rars_tools", 0x10000);
    sysbus_init_mmio(dev, &s->iomem);
}

static void rars_tools_realize(DeviceState *dev, Error **errp)
{
    RarsToolsState *s = RARS_TOOLS(dev);

    s->con = graphic_console_init(dev, 0, &rars_tools_graphic_ops, s);
}

static void rars_tools_reset(DeviceState *d)
{
    RarsToolsState *s = RARS_TOOLS(d);

    s->segments_left = 0;
    s->segments_right = 0;
    s->state = REDRAW_DISPLAYS | REDRAW_BACKGROUND;
    qemu_console_resize(s->con, 120, 100);
}

static void rars_tools_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->desc = "Rars Tools display",
    dc->vmsd = &vmstate_rars_tools;
    dc->reset = rars_tools_reset;
    dc->realize = rars_tools_realize;
}

static const TypeInfo rars_tools_info = {
    .name          = TYPE_RARS_TOOLS,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(RarsToolsState),
    .instance_init = rars_tools_init,
    .class_init    = rars_tools_class_init,
};

static void rars_tools_register(void)
{
    type_register_static(&rars_tools_info);
}

type_init(rars_tools_register);
