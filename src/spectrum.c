/* SPDX-License-Identifier: Apache-2.0 */
#include "spectrum.h"
#include <string.h>
static uint16_t smaller(uint32_t a, uint32_t b)
{
	return (uint16_t)(a < b ? a : b);
}
void spectrum_init(struct spectrum *s)
{
	memset(s, 0, sizeof(*s));
	s->cursor = 12;
}
void spectrum_record(struct spectrum *s, unsigned int channel, bool hit)
{
	if (channel >= SPECTRUM_CHANNELS) {
		return;
	}
	if (s->samples[channel] == SPECTRUM_WINDOW_SAMPLES) {
		s->hits[channel] -= (s->history[channel] >> 15) & 1U;
	} else {
		s->samples[channel]++;
	}
	s->history[channel] = (uint16_t)((s->history[channel] << 1) | (hit ? 1U : 0U));
	s->hits[channel] += hit ? 1U : 0U;
}
bool spectrum_is_valid(const struct spectrum *s, unsigned int channel)
{
	return channel < SPECTRUM_CHANNELS &&
	       s->samples[channel] == SPECTRUM_WINDOW_SAMPLES;
}
unsigned int spectrum_percent(const struct spectrum *s, unsigned int channel)
{
	return spectrum_is_valid(s, channel) ?
	       (s->hits[channel] * 100U + SPECTRUM_WINDOW_SAMPLES / 2U) /
	       SPECTRUM_WINDOW_SAMPLES : 0;
}
void spectrum_animate(struct spectrum *s, uint32_t now_ms, uint32_t elapsed_ms)
{
	/* Fixed-point percent, independent of LCD refresh rate. */
	uint32_t dt = elapsed_ms > 200U ? 200U : elapsed_ms;
	uint32_t attack = 400U * SPECTRUM_ONE_PERCENT * dt / 1000U;
	uint32_t release = 120U * SPECTRUM_ONE_PERCENT * dt / 1000U;
	uint32_t fall = 25U * SPECTRUM_ONE_PERCENT * dt / 1000U;
	for (unsigned int c = 0; c < SPECTRUM_CHANNELS; ++c) {
		if (!spectrum_is_valid(s, c)) {
			continue;
		}
		uint32_t target = (uint32_t)s->hits[c] * SPECTRUM_FULL_SCALE /
				  SPECTRUM_WINDOW_SAMPLES;
		if (target > s->level[c]) {
			s->level[c] += smaller(target - s->level[c], attack);
		} else {
			s->level[c] -= smaller(s->level[c] - target, release);
		}
		uint16_t reference = target > s->level[c] ? (uint16_t)target : s->level[c];
		if (reference > 0 && reference >= s->peak[c]) {
			s->peak[c] = reference;
			s->peak_until[c] = now_ms + SPECTRUM_PEAK_HOLD_MS;
		} else if ((int32_t)(now_ms - s->peak_until[c]) >= 0) {
			s->peak[c] -= smaller(s->peak[c], fall);
			if (s->peak[c] < reference) {
				s->peak[c] = reference;
			}
		}
	}
}
void spectrum_cursor_right(struct spectrum *s)
{
	s->cursor = (uint8_t)((s->cursor + 1U) % SPECTRUM_CHANNELS);
}
