#include "ssd1306.h"
#include "font.h"
#include <string.h>
#include <stdlib.h>

static void ssd1306_send_cmd(ssd1306_t *p, uint8_t cmd) {
    uint8_t buf[2] = {0x00, cmd};
    i2c_write_blocking(p->i2c_i, p->address, buf, 2, false);
}

static void ssd1306_send_cmd_list(ssd1306_t *p, uint8_t *buf, int num) {
    for (int i = 0; i < num; i++)
        ssd1306_send_cmd(p, buf[i]);
}

static void ssd1306_send_buffer(ssd1306_t *p) {
    uint8_t *buf = (uint8_t*)malloc(p->bufsize + 1);
    buf[0] = 0x40; // Control byte
    memcpy(buf + 1, p->buffer, p->bufsize);
    i2c_write_blocking(p->i2c_i, p->address, buf, p->bufsize + 1, false);
    free(buf);
}

bool ssd1306_init(ssd1306_t *p, uint16_t width, uint16_t height, uint8_t address, i2c_inst_t *i2c_instance) {
    p->width = width;
    p->height = height;
    p->pages = height / 8;
    p->address = address;
    p->i2c_i = i2c_instance;
    p->bufsize = p->pages * p->width;
    p->buffer = (uint8_t*)malloc(p->bufsize);
    if (p->buffer == NULL) return false;

    uint8_t cmds[] = {
        SET_DISP | 0x00, // off
        SET_MEM_ADDR, 0x00, // horizontal addressing mode
        SET_DISP_START_LINE | 0x00,
        SET_SEG_REMAP | 0x01,
        SET_MUX_RATIO, height - 1,
        SET_COM_OUT_DIR | 0x08,
        SET_DISP_OFFSET, 0x00,
        SET_COM_PIN_CFG, 0x12,
        SET_DISP_CLK_DIV, 0x80,
        SET_PRECHARGE, 0xF1,
        SET_VCOM_DESEL, 0x30,
        SET_CONTRAST, 0xFF,
        SET_ENTIRE_ON,
        SET_NORM_INV,
        SET_CHARGE_PUMP, 0x14,
        SET_DISP | 0x01, // on
    };
    ssd1306_send_cmd_list(p, cmds, sizeof(cmds));
    ssd1306_clear(p);
    ssd1306_show(p);
    return true;
}

void ssd1306_deinit(ssd1306_t *p) {
    free(p->buffer);
}

void ssd1306_show(ssd1306_t *p) {
    ssd1306_send_cmd(p, SET_COL_ADDR);
    ssd1306_send_cmd(p, 0);
    ssd1306_send_cmd(p, p->width - 1);
    ssd1306_send_cmd(p, SET_PAGE_ADDR);
    ssd1306_send_cmd(p, 0);
    ssd1306_send_cmd(p, p->pages - 1);
    ssd1306_send_buffer(p);
}

void ssd1306_clear(ssd1306_t *p) {
    memset(p->buffer, 0, p->bufsize);
}

void ssd1306_draw_pixel(ssd1306_t *p, uint32_t x, uint32_t y) {
    if (x >= p->width || y >= p->height) return;
    p->buffer[x + (y / 8) * p->width] |= 1 << (y % 8);
}

void ssd1306_draw_string(ssd1306_t *p, uint32_t x, uint32_t y, uint32_t scale, const char *s) {
    for (int i = 0; *s; s++, i++) {
        ssd1306_draw_char_with_font(p, x + i * (font_8x5[1] + 1) * scale, y, scale, font_8x5, *s);
    }
}

void ssd1306_draw_char_with_font(ssd1306_t *p, uint32_t x, uint32_t y, uint32_t scale, const uint8_t *font, char c) {
    if (c < font[3] || c > font[4]) return;
    uint8_t height = font[0];
    uint8_t width = font[1];
    uint32_t font_idx = 5 + (c - font[3]) * width;
    for (uint8_t i = 0; i < width; i++) {
        uint8_t line = font[font_idx + i];
        for (uint8_t j = 0; j < height; j++, line >>= 1) {
            if (line & 1) {
                if (scale == 1) {
                    ssd1306_draw_pixel(p, x + i, y + j);
                } else {
                    ssd1306_draw_square(p, x + i * scale, y + j * scale, scale, scale);
                }
            }
        }
    }
}

void ssd1306_draw_square(ssd1306_t *p, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    for (uint32_t i = 0; i < width; i++)
        for (uint32_t j = 0; j < height; j++)
            ssd1306_draw_pixel(p, x + i, y + j);
}