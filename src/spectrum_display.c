/* SPDX-License-Identifier: Apache-2.0 */
#include "spectrum_display.h"
#include <errno.h>
#include <stdio.h>
#include <zephyr/drivers/display.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>
#define WIDTH 160U
#define HEIGHT 80U
#define TILE_ROWS 4U
#define LEFT 20U
#define RIGHT 157U
#define PLOT_WIDTH (RIGHT - LEFT + 1U)
#define MID_CHANNEL ((SPECTRUM_CHANNELS / 2U) / 10U * 10U)
#define MID_X (LEFT + (MID_CHANNEL * PLOT_WIDTH + SPECTRUM_CHANNELS - 1U) / SPECTRUM_CHANNELS)
BUILD_ASSERT(SPECTRUM_CHANNELS <= PLOT_WIDTH, "Need at least one pixel per channel");
#define TOP 20U
#define BASE 66U
#define PLOT_HEIGHT (BASE - TOP)
#define BLACK 0x0000
#define WHITE 0xffff
#define GREEN 0x07e0
#define YELLOW 0xffe0
#define RED 0xf800
#define GRID 0x18c3
#define GRAY 0x7bef
/* Application-owned 5x7 font; each byte is a row, low five bits used. */
static const char alphabet[] = "0123456789%ACEHIKLMNOPRSTUVWXYZ -";
static const uint8_t glyphs[][7] = {
	{14,17,19,21,25,17,14}, {4,12,4,4,4,4,14},
	{14,17,1,2,4,8,31}, {30,1,1,14,1,1,30},
	{2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
	{14,16,16,30,17,17,14}, {31,1,2,4,8,8,8},
	{14,17,17,14,17,17,14}, {14,17,17,15,1,1,14},
	{17,18,2,4,8,9,17}, /* % */
	{14,17,17,31,17,17,17}, /* A */
	{14,17,16,16,16,17,14}, /* C */
	{31,16,16,30,16,16,31}, /* E */
	{17,17,17,31,17,17,17}, /* H */
	{14,4,4,4,4,4,14}, /* I */
	{17,18,20,24,20,18,17}, /* K */
	{16,16,16,16,16,16,31}, /* L */
	{17,27,21,21,17,17,17}, /* M */
	{17,25,25,21,19,19,17}, /* N */
	{14,17,17,17,17,17,14}, /* O */
	{30,17,17,30,16,16,16}, /* P */
	{30,17,17,30,20,18,17}, /* R */
	{15,16,16,14,1,1,30}, /* S */
	{31,4,4,4,4,4,4}, /* T */
	{17,17,17,17,17,17,14}, /* U */
	{17,17,17,17,17,10,4}, /* V */
	{17,17,17,21,21,27,17}, /* W */
	{17,17,10,4,10,17,17}, /* X */
	{17,17,10,4,4,4,4}, /* Y */
	{31,1,2,4,8,16,31}, /* Z */
	{0,0,0,0,0,0,0}, /* space */
	{0,0,0,31,0,0,0}, /* - */
};
BUILD_ASSERT(ARRAY_SIZE(glyphs) == sizeof(alphabet) - 1);
static const struct device *lcd;
static bool bgr;
static bool active;
static uint16_t next_y;
static uint8_t tile[WIDTH * TILE_ROWS * 2U];
static struct {
	uint8_t heights[SPECTRUM_CHANNELS];
	uint8_t peaks[SPECTRUM_CHANNELS];
	uint16_t cursor_x;
	int radio_error;
	bool button_ok;
	bool warmup;
	char header[28];
	char axis_first[6];
	char axis_mid[6];
	char axis_last[6];
} view;
static void pixel(unsigned int row, unsigned int x, uint16_t color)
{
	if (bgr) {
		color = ((color & 0x001fU) << 11) | (color & 0x07e0U) |
			((color & 0xf800U) >> 11);
	}
	sys_put_be16(color, &tile[(row * WIDTH + x) * 2U]);
}
static const uint8_t *glyph(char ch)
{
	for (size_t i = 0; i < sizeof(alphabet) - 1U; ++i) {
		if (alphabet[i] == ch) {
			return glyphs[i];
		}
	}
	return glyphs[sizeof(alphabet) - 3U]; /* space */
}
static void text_row(unsigned int row, unsigned int y, unsigned int x0,
		     unsigned int y0, const char *text, uint16_t color)
{
	if (y < y0 || y >= y0 + 7U) {
		return;
	}
	for (unsigned int i = 0; text[i] != '\0'; ++i) {
		unsigned int x = x0 + i * 6U;
		if (x >= WIDTH) {
			break;
		}
		uint8_t bits = glyph(text[i])[y - y0];
		for (unsigned int bit = 0; bit < 5U && x + bit < WIDTH; ++bit) {
			if (bits & (1U << (4U - bit))) {
				pixel(row, x + bit, color);
			}
		}
	}
}
static void draw_row(unsigned int row, unsigned int y)
{
	for (unsigned int x = 0; x < WIDTH; ++x) {
		uint16_t color = BLACK;
		if (x >= LEFT && x <= RIGHT && y >= TOP && y < BASE) {
			unsigned int c = (x - LEFT) * SPECTRUM_CHANNELS / PLOT_WIDTH;
			if (y == TOP || y == TOP + PLOT_HEIGHT / 2U ||
			    x == LEFT ||
			    x == MID_X || x == RIGHT) {
				color = GRID;
			}
			if (BASE - y <= view.heights[c]) {
				color = GREEN;
			}
			if (view.peaks[c] > 0 && BASE - y == view.peaks[c]) {
				color = YELLOW;
			}
			if (x == view.cursor_x && (y - TOP) % 3U == 0) {
				color = WHITE;
			}
		}
		if ((y == BASE && x >= LEFT && x <= RIGHT) ||
		    (x == LEFT - 1U && y >= TOP && y <= BASE)) {
			color = GRAY;
		}
		if (y > BASE && y <= BASE + 3U) {
			unsigned int radius = y - BASE - 1U;
			if (x + radius >= view.cursor_x && x <= view.cursor_x + radius) {
				color = WHITE;
			}
		}
		if (x >= 153U && x < 158U && y >= 1U && y < 6U) {
			color = view.radio_error ? RED : GREEN;
		}
		pixel(row, x, color);
	}
	text_row(row, y, 1, 0, view.header, view.radio_error ? RED : WHITE);
	text_row(row, y, LEFT, 10, "LIVE", GREEN);
	text_row(row, y, 53, 10, "PEAK", YELLOW);
	text_row(row, y, 91, 10, !view.button_ok ? "KEY ERR" :
		 view.warmup ? "WARMUP" : "ACT %", GRAY);
	text_row(row, y, 0, 17, "100", GRAY);
	text_row(row, y, 6, 40, "50", GRAY);
	text_row(row, y, 12, 63, "0", GRAY);
	text_row(row, y, LEFT, 73, view.axis_first, GRAY);
	text_row(row, y, MID_X - 11U, 73, view.axis_mid, GRAY);
	text_row(row, y, 134, 73, view.axis_last, GRAY);
}
int spectrum_display_init(const struct device *panel)
{
	struct display_capabilities caps = {0};
	if (!device_is_ready(panel)) {
		return -ENODEV;
	}
	display_get_capabilities(panel, &caps);
	if (caps.x_resolution != WIDTH || caps.y_resolution != HEIGHT ||
	    (caps.current_pixel_format != PIXEL_FORMAT_RGB_565 &&
	     caps.current_pixel_format != PIXEL_FORMAT_BGR_565)) {
		return -ENOTSUP;
	}
	lcd = panel;
	snprintf(view.axis_first, sizeof(view.axis_first), "%u", SPECTRUM_FIRST_MHZ);
	snprintf(view.axis_mid, sizeof(view.axis_mid), "%u", SPECTRUM_FIRST_MHZ + MID_CHANNEL);
	snprintf(view.axis_last, sizeof(view.axis_last), "%u", SPECTRUM_LAST_MHZ);
	bgr = caps.current_pixel_format == PIXEL_FORMAT_BGR_565;
	active = false;
	return display_blanking_off(panel);
}
void spectrum_display_begin(const struct spectrum *s, int radio_error, bool button_ok)
{
	view.radio_error = radio_error;
	view.button_ok = button_ok;
	view.warmup = false;
	for (unsigned int c = 0; c < SPECTRUM_CHANNELS; ++c) {
		view.heights[c] = (uint8_t)((s->level[c] * PLOT_HEIGHT +
			SPECTRUM_FULL_SCALE - 1U) / SPECTRUM_FULL_SCALE);
		view.peaks[c] = (uint8_t)((s->peak[c] * PLOT_HEIGHT +
			SPECTRUM_FULL_SCALE - 1U) / SPECTRUM_FULL_SCALE);
		view.warmup |= !spectrum_is_valid(s, c);
	}
	unsigned int x0 = (s->cursor * PLOT_WIDTH + SPECTRUM_CHANNELS - 1U) /
			  SPECTRUM_CHANNELS;
	unsigned int x1 = ((s->cursor + 1U) * PLOT_WIDTH + SPECTRUM_CHANNELS - 1U) /
			  SPECTRUM_CHANNELS;
	view.cursor_x = LEFT + (x0 + x1 - 1U) / 2U;
	if (radio_error) {
		snprintf(view.header, sizeof(view.header), "SI24 ERR %d", radio_error);
	} else if (!spectrum_is_valid(s, s->cursor)) {
		snprintf(view.header, sizeof(view.header), "%uMHZ --%% P--%%",
			 SPECTRUM_FIRST_MHZ + s->cursor);
	} else {
		snprintf(view.header, sizeof(view.header), "%uMHZ %u%% P%u%%",
			 SPECTRUM_FIRST_MHZ + s->cursor, spectrum_percent(s, s->cursor),
			 (s->peak[s->cursor] + SPECTRUM_ONE_PERCENT / 2U) /
			 SPECTRUM_ONE_PERCENT);
	}
	next_y = 0;
	active = true;
}
bool spectrum_display_busy(void)
{
	return active;
}
int spectrum_display_step(void)
{
	if (!active) {
		return 0;
	}
	unsigned int rows = MIN(TILE_ROWS, HEIGHT - next_y);
	for (unsigned int r = 0; r < rows; ++r) {
		draw_row(r, next_y + r);
	}
	const struct display_buffer_descriptor desc = {
		.buf_size = WIDTH * rows * 2U, .width = WIDTH,
		.height = rows, .pitch = WIDTH,
	};
	int ret = display_write(lcd, 0, next_y, &desc, tile);
	if (ret < 0) {
		active = false;
		return ret;
	}
	next_y += rows;
	active = next_y < HEIGHT;
	return 0;
}
