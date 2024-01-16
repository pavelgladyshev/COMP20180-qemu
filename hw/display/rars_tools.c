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

typedef enum {
    RARS_TOOLS_RGB_HEIGHT = 32,
    RARS_TOOLS_RGB_WIDTH = 32,
    RARS_TOOLS_RGB_ADDR = 0x4000,
    RARS_TOOLS_RGB_PIXEL_SIZE = 8,
} rgb_display_constants;

typedef enum {
    RARS_TOOLS_MONO_HEIGHT = 32,
//    RARS_TOOLS_MONO_WIDTH = 32,  - assumed
    RARS_TOOLS_MONO_ADDR = 0x8000,
    RARS_TOOLS_MONO_PIXEL_SIZE = 8,
} mono_display_constants;

struct RarsToolsState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    uint16_t segments_left;
    uint16_t segments_right;
    uint32_t rgb_data[RARS_TOOLS_RGB_HEIGHT * RARS_TOOLS_RGB_WIDTH + 2];  // 2 words at the end create buffer zone for misaligned reads and writes
    uint32_t mono_data[RARS_TOOLS_MONO_HEIGHT + 2];  // 2 words at the end create buffer zone for misaligned reads and writes. 
    QemuConsole *con;
    screen_state_t state;
};

static uint64_t rars_tools_read(void *opaque, hwaddr addr,
                              unsigned int size)
{
    RarsToolsState *s = opaque;
    uint64_t val;

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

    if ((addr >= RARS_TOOLS_RGB_ADDR) && (addr < RARS_TOOLS_RGB_HEIGHT*RARS_TOOLS_RGB_WIDTH*4+RARS_TOOLS_RGB_ADDR)){
        int rgb_addr = addr - RARS_TOOLS_RGB_ADDR;
        int index = rgb_addr / 4;  // index of RGB word in rgb_data
        int biw = rgb_addr % 4;    // byte in word (0 - rightmost, assuming LE)
        switch (size) {
            case 1:
                val = *(((uint8_t *)&(s->rgb_data[index]))+biw);
                break;
            case 2:
                val = *((uint16_t *)(((uint8_t *)&(s->rgb_data[index]))+biw));
                break;
            case 4:
                val = *((uint32_t *)(((uint8_t *)&(s->rgb_data[index]))+biw));
                break;
            case 8:
                val = *((uint64_t *)(((uint8_t *)&(s->rgb_data[index]))+biw));
                break;
        }
    } else if ((addr >= RARS_TOOLS_MONO_ADDR) && (addr < RARS_TOOLS_MONO_HEIGHT*4+RARS_TOOLS_MONO_ADDR)) {
        int mono_addr = addr - RARS_TOOLS_MONO_ADDR;
        int index = mono_addr / 4;  // index of monochrome display memory word in mono_data
        int biw = mono_addr % 4;    // byte in word (0 - rightmost, assuming LE)
        switch (size) {
            case 1:
                val = *(((uint8_t *)&(s->mono_data[index]))+biw);
                break;
            case 2:
                val = *((uint16_t *)(((uint8_t *)&(s->mono_data[index]))+biw));
                break;
            case 4:
                val = *((uint32_t *)(((uint8_t *)&(s->mono_data[index]))+biw));
                break;
            case 8:
                val = *((uint64_t *)(((uint8_t *)&(s->mono_data[index]))+biw));
                break;
        }
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


    if ((addr >= RARS_TOOLS_RGB_ADDR) && (addr < RARS_TOOLS_RGB_HEIGHT*RARS_TOOLS_RGB_WIDTH*4+RARS_TOOLS_RGB_ADDR)){
        int rgb_addr = addr - RARS_TOOLS_RGB_ADDR;
        int index = rgb_addr / 4;  // index of RGB word in rgb_data
        int biw = rgb_addr % 4;    // byte in word (0 - rightmost, assuming LE)
        switch (size) {
            case 1:
                *(((uint8_t *)&(s->rgb_data[index]))+biw) = (uint8_t)val;
                break;
            case 2:
                *((uint16_t *)(((uint8_t *)&(s->rgb_data[index]))+biw)) = (uint16_t)val;
                break;
            case 4:
                *((uint32_t *)(((uint8_t *)&(s->rgb_data[index]))+biw)) = (uint32_t)val;
                break;
            case 8:
                *((uint64_t *)(((uint8_t *)&(s->rgb_data[index]))+biw)) = (uint64_t)val;
                break;
        }
    } else if ((addr >= RARS_TOOLS_MONO_ADDR) && (addr < RARS_TOOLS_MONO_HEIGHT*4+RARS_TOOLS_MONO_ADDR)){
        int mono_addr = addr - RARS_TOOLS_MONO_ADDR;
        int index = mono_addr / 4;  // index of monochrome display memory word in mono_data
        int biw = mono_addr % 4;    // byte in word (0 - rightmost, assuming LE)
        switch (size) {
            case 1:
                *(((uint8_t *)&(s->mono_data[index]))+biw) = (uint8_t)val;
                break;
            case 2:
                *((uint16_t *)(((uint8_t *)&(s->mono_data[index]))+biw)) = (uint16_t)val;
                break;
            case 4:
                *((uint32_t *)(((uint8_t *)&(s->mono_data[index]))+biw)) = (uint32_t)val;
                break;
            case 8:
                *((uint64_t *)(((uint8_t *)&(s->mono_data[index]))+biw)) = (uint64_t)val;
                break;
        }
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


static void rars_tools_draw_rgb_display(DisplaySurface *surface, 
                                        uint32_t rgb_data[], 
                                        int display_top_left_x, 
                                        int display_top_left_y) 
{
    int row,col;
    uint32_t *next_rgb_pixel = rgb_data;

    int bpp, stride;
    uint8_t *d, *line_start, *next_pixel, *next_subpixel;
    uint32_t next_pixel_color;
    
    // Get parameters of QEMU display surface.
    bpp = (surface_bits_per_pixel(surface) + 7) >> 3;  // number of bytes per pixel on QEMU display
    stride = surface_stride(surface);                  // number of bytes per line on QEMU display
    d = surface_data(surface);                         // start of QEMU display frame buffer in memory

    // copy pixel data from rgb_data[] array to specified area of QEMU display
    // converting pixel data according to specs of QEMU display
    line_start = d + stride * display_top_left_y + bpp * display_top_left_x;

    for (row=0; row < RARS_TOOLS_RGB_HEIGHT; row++)
    {
        next_pixel = line_start;
        for (col=0; col < RARS_TOOLS_RGB_WIDTH; col++)
        {
            // draw next pixel
            switch (bpp) {
            case 1:
                next_pixel_color = rgb_to_pixel8(*(((uint8_t*)next_rgb_pixel)+2),
                                                 *(((uint8_t*)next_rgb_pixel)+1),
                                                 *(((uint8_t*)next_rgb_pixel)+0));
                next_subpixel = next_pixel; 
                next_pixel += RARS_TOOLS_RGB_PIXEL_SIZE * bpp;
                for (int i = 0; i < RARS_TOOLS_RGB_PIXEL_SIZE; i++)
                {
                    for (int j = 0; j < RARS_TOOLS_RGB_PIXEL_SIZE; j++)
                    {
                        *((uint8_t *)next_subpixel) = next_pixel_color;
                        next_subpixel += bpp; 
                    }
                    next_subpixel += stride - RARS_TOOLS_RGB_PIXEL_SIZE * bpp;
                }
                break;
            case 2:
                next_pixel_color = rgb_to_pixel16(*(((uint8_t*)next_rgb_pixel)+2),
                                                  *(((uint8_t*)next_rgb_pixel)+1),
                                                  *(((uint8_t*)next_rgb_pixel)+0));
                next_subpixel = next_pixel; 
                next_pixel += RARS_TOOLS_RGB_PIXEL_SIZE * bpp;
                for (int i = 0; i < RARS_TOOLS_RGB_PIXEL_SIZE; i++)
                {
                    for (int j = 0; j < RARS_TOOLS_RGB_PIXEL_SIZE; j++)
                    {
                        *((uint16_t *)next_subpixel) = next_pixel_color;
                        next_subpixel += bpp; 
                    }
                    next_subpixel += stride - RARS_TOOLS_RGB_PIXEL_SIZE * bpp;
                }
                break;
            case 4:
                next_pixel_color = rgb_to_pixel32(*(((uint8_t*)next_rgb_pixel)+2),
                                                  *(((uint8_t*)next_rgb_pixel)+1),
                                                  *(((uint8_t*)next_rgb_pixel)+0));
                next_subpixel = next_pixel; 
                next_pixel += RARS_TOOLS_RGB_PIXEL_SIZE * bpp;
                for (int i = 0; i < RARS_TOOLS_RGB_PIXEL_SIZE; i++)
                {
                    for (int j = 0; j < RARS_TOOLS_RGB_PIXEL_SIZE; j++)
                    {
                        *((uint32_t *)next_subpixel) = next_pixel_color;
                        next_subpixel += bpp; 
                    }
                    next_subpixel += stride - RARS_TOOLS_RGB_PIXEL_SIZE * bpp;
                }
                break;
            }
            next_rgb_pixel++;
        }
        line_start += stride * RARS_TOOLS_RGB_PIXEL_SIZE;
    }
}

static void rars_tools_draw_mono_display(DisplaySurface *surface, 
                                        uint32_t *mono_data,        // pointer to the start of array of 32 words 
                                        int display_top_left_x, 
                                        int display_top_left_y) 
{
    int row,col;

    int bpp, stride;
    uint8_t *d, *line_start, *next_pixel, *next_subpixel;
    uint32_t color_on, color_off, next_mono_line, next_pixel_color;
    
    // Get parameters of QEMU display surface.
    bpp = (surface_bits_per_pixel(surface) + 7) >> 3;  // number of bytes per pixel on QEMU display
    stride = surface_stride(surface);                  // number of bytes per line on QEMU display
    d = surface_data(surface);                         // start of QEMU display frame buffer in memory

    // set on and off colours of monochrome pixels according to bits per pixel settings of the surface
    switch (surface_bits_per_pixel(surface)) {
        case 8:
            color_on = rgb_to_pixel8(0x00, 0xff, 0x00);
            color_off = rgb_to_pixel8(0x00, 0x00, 0x00);
            break;
        case 15:
            color_on = rgb_to_pixel15(0x00, 0xff, 0x00);
            color_off = rgb_to_pixel15(0x00, 0x00, 0x00);
            break;
        case 16:
            color_on = rgb_to_pixel16(0x00, 0xff, 0x00);
            color_off = rgb_to_pixel16(0x00, 0x00, 0x00);
            break;
        case 24:
            color_on = rgb_to_pixel24(0x00, 0xff, 0x00);
            color_off = rgb_to_pixel24(0x00, 0x00, 0x00);
            break;
        case 32:
            color_on = rgb_to_pixel32(0x00, 0xff, 0x00);
            color_off = rgb_to_pixel32(0x00, 0x00, 0x00);
            break;
        default:
            return;
    }

    // copy pixel data from mono_data[] array to specified area of QEMU display
    // converting pixel data according to specs of QEMU display
    
    line_start = d + stride * display_top_left_y + bpp * display_top_left_x;

    next_mono_line = *mono_data++;

    for (row=0; row < RARS_TOOLS_RGB_HEIGHT; row++)
    {
        next_pixel = line_start;
        for (col=0; col < RARS_TOOLS_RGB_WIDTH; col++)
        {
            // get next pixel color by checking the value of the corresponding bit 
            // in the bitmap display line

            next_pixel_color = (0x80000000 & next_mono_line) ? color_on : color_off;
            next_mono_line <<=1;

            // draw next RARS Monochrome display pixel as a small square 
            // RARS_TOOLS_MONO_PIXEL_SIZE wide and tall (in QEMU display pixels).
            switch (bpp) {
            case 1:
                next_subpixel = next_pixel; 
                next_pixel += RARS_TOOLS_MONO_PIXEL_SIZE * bpp;
                for (int i = 0; i < RARS_TOOLS_MONO_PIXEL_SIZE; i++)
                {
                    for (int j = 0; j < RARS_TOOLS_MONO_PIXEL_SIZE; j++)
                    {
                        *((uint8_t *)next_subpixel) = next_pixel_color;
                        next_subpixel += bpp; 
                    }
                    next_subpixel += stride - RARS_TOOLS_MONO_PIXEL_SIZE * bpp;
                }
                break;
            case 2:
                next_subpixel = next_pixel; 
                next_pixel += RARS_TOOLS_MONO_PIXEL_SIZE * bpp;
                for (int i = 0; i < RARS_TOOLS_MONO_PIXEL_SIZE; i++)
                {
                    for (int j = 0; j < RARS_TOOLS_MONO_PIXEL_SIZE; j++)
                    {
                        *((uint16_t *)next_subpixel) = next_pixel_color;
                        next_subpixel += bpp; 
                    }
                    next_subpixel += stride - RARS_TOOLS_MONO_PIXEL_SIZE * bpp;
                }
                break;
            case 4:
                next_subpixel = next_pixel; 
                next_pixel += RARS_TOOLS_MONO_PIXEL_SIZE * bpp;
                for (int i = 0; i < RARS_TOOLS_MONO_PIXEL_SIZE; i++)
                {
                    for (int j = 0; j < RARS_TOOLS_MONO_PIXEL_SIZE; j++)
                    {
                        *((uint32_t *)next_subpixel) = next_pixel_color;
                        next_subpixel += bpp; 
                    }
                    next_subpixel += stride - RARS_TOOLS_MONO_PIXEL_SIZE * bpp;
                }
                break;
            }
        }
        line_start += stride * RARS_TOOLS_MONO_PIXEL_SIZE;
        next_mono_line = *mono_data++;
    }
}


static void rars_tools_update_display(void *opaque)
{
    RarsToolsState *s = opaque;
    DisplaySurface *surface = qemu_console_surface(s->con);
    uint8_t *d1;
    uint32_t color_segment, color_led, color_bg;
    int y, x, bpp;

    if (s->state & REDRAW_BACKGROUND) {
        /* clear screen */
        bpp = (surface_bits_per_pixel(surface) + 7) >> 3;

        /* set background colour accosting to bits per pixel settings of the surface*/
        switch (surface_bits_per_pixel(surface)) {
        case 8:
            color_bg = rgb_to_pixel8(0x20, 0x20, 0x20);
            break;
        case 15:
            color_bg = rgb_to_pixel15(0x20, 0x20, 0x20);
            break;
        case 16:
            color_bg = rgb_to_pixel16(0x20, 0x20, 0x20);
            break;
        case 24:
            color_bg = rgb_to_pixel24(0x20, 0x20, 0x20);
            break;
        case 32:
            color_bg = rgb_to_pixel32(0x20, 0x20, 0x20);
            break;
        default:
            return;
        }

        d1 = surface_data(surface);
        switch (bpp) {
            case 1:
                for (y = 0; y < surface_height(surface); y++) {
                    memset(d1, color_bg, surface_width(surface) * bpp);
                    d1 += surface_stride(surface);
                }
                break;
            case 2:
                for (y = 0; y < surface_height(surface); y++) {
                    for (x = 0; x < surface_width(surface); x++, d1 += bpp) {
                        *((uint16_t *)d1) = (uint16_t) color_bg;
                    }
                }
                break;
            case 4:
                for (y = 0; y < surface_height(surface); y++) {
                    for (x = 0; x < surface_width(surface); x++, d1 += bpp) {
                        *((uint32_t *)d1) = (uint32_t) color_bg;
                    }
                }
                break;
        }
    }

    if (s->state & REDRAW_DISPLAYS) {
        /* set colors according to bpp */
        switch (surface_bits_per_pixel(surface)) {
        case 8:
            color_segment = rgb_to_pixel8(0xff, 0x00, 0x00);
            color_led = rgb_to_pixel8(0xff, 0x00, 0x00);
            break;
        case 15:
            color_segment = rgb_to_pixel15(0xff, 0x00, 0x00);
            color_led = rgb_to_pixel15(0xff, 0x00, 0x00);
            break;
        case 16:
            color_segment = rgb_to_pixel16(0xff, 0x00, 0x00);
            color_led = rgb_to_pixel16(0xff, 0x00, 0x00);
            break;
        case 24:
            color_segment = rgb_to_pixel24(0xff, 0x00, 0x00);
            color_led = rgb_to_pixel24(0xff, 0x00, 0x00);
            break;
        case 32:
            color_segment = rgb_to_pixel32(0xff, 0x00, 0x00);
            color_led = rgb_to_pixel32(0xff, 0x00, 0x00);
            break;
        default:
            return;
        }

        rars_tools_draw_7segment_display(surface, color_segment, color_led, s->segments_left, 30, 120);
        rars_tools_draw_7segment_display(surface, color_segment, color_led, s->segments_right, 90, 120);
        rars_tools_draw_rgb_display(surface,s->rgb_data, 180, 40);
        rars_tools_draw_mono_display(surface,s->mono_data, 476, 40);
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
    memset(s->rgb_data,0,RARS_TOOLS_RGB_HEIGHT*RARS_TOOLS_RGB_WIDTH*sizeof(uint32_t));
    memset(s->mono_data,0,RARS_TOOLS_MONO_HEIGHT*sizeof(uint32_t));

    s->state = REDRAW_DISPLAYS | REDRAW_BACKGROUND;
    qemu_console_resize(s->con, 772, 340);
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
