/* SPDX-License-Identifier: Apache-2.0 */
#ifndef APP_SPECTRUM_H_
#define APP_SPECTRUM_H_
#include <stdbool.h>
#include <stdint.h>
#define SPECTRUM_FIRST_MHZ 2400U
#define SPECTRUM_CHANNELS 126U
#define SPECTRUM_LAST_MHZ (SPECTRUM_FIRST_MHZ + SPECTRUM_CHANNELS - 1U)
#define SPECTRUM_WINDOW_SAMPLES 16U
#define SPECTRUM_ONE_PERCENT 256U
#define SPECTRUM_FULL_SCALE (100U * SPECTRUM_ONE_PERCENT)
#define SPECTRUM_PEAK_HOLD_MS 1000U
struct spectrum {
	uint16_t history[SPECTRUM_CHANNELS];
	uint8_t hits[SPECTRUM_CHANNELS];
	uint8_t samples[SPECTRUM_CHANNELS];
	uint16_t level[SPECTRUM_CHANNELS];
	uint16_t peak[SPECTRUM_CHANNELS];
	uint32_t peak_until[SPECTRUM_CHANNELS];
	uint8_t cursor;
};
void spectrum_init(struct spectrum *s);
void spectrum_record(struct spectrum *s, unsigned int channel, bool hit);
bool spectrum_is_valid(const struct spectrum *s, unsigned int channel);
unsigned int spectrum_percent(const struct spectrum *s, unsigned int channel);
void spectrum_animate(struct spectrum *s, uint32_t now_ms, uint32_t elapsed_ms);
void spectrum_cursor_right(struct spectrum *s);
#endif
