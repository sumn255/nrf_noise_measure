/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "spectrum.h"
#include "button_filter.h"

static void fill(struct spectrum *s, unsigned int channel, bool hit)
{
	for (unsigned int i = 0; i < SPECTRUM_WINDOW_SAMPLES; ++i) {
		spectrum_record(s, channel, hit);
	}
}

static void test_rolling_activity(void)
{
	struct spectrum s;
	const unsigned int last = SPECTRUM_CHANNELS - 1U;
	spectrum_init(&s);
	assert(SPECTRUM_LAST_MHZ == 2525U);
	assert(last == 125U);
	assert(s.cursor == 12);
	for (unsigned int i = 0; i < 15; ++i) {
		spectrum_record(&s, last, true);
	}
	assert(!spectrum_is_valid(&s, last));
	assert(spectrum_percent(&s, last) == 0);
	spectrum_animate(&s, 100, 100);
	assert(s.peak[last] == 0); /* No spurious 100% peak during warm-up. */
	spectrum_record(&s, last, true);
	assert(spectrum_percent(&s, last) == 100);
	for (unsigned int i = 0; i < 8; ++i) {
		spectrum_record(&s, last, false);
	}
	assert(spectrum_percent(&s, last) == 50);
	for (unsigned int i = 0; i < 8; ++i) {
		spectrum_record(&s, last, false);
	}
	assert(spectrum_percent(&s, last) == 0);
	assert(s.samples[last] == 16 && s.hits[last] == 0);
	assert(s.samples[0] == 0); /* Channels remain independent. */
	for (unsigned int i = 0; i < 1000; ++i) {
		spectrum_record(&s, 0, i % 2U != 0);
		if (i >= 15) {
			assert(spectrum_percent(&s, 0) == 50);
		}
	}
	struct spectrum before = s;
	spectrum_record(&s, SPECTRUM_CHANNELS, true);
	assert(memcmp(&s, &before, sizeof(s)) == 0);
	assert(!spectrum_is_valid(&s, SPECTRUM_CHANNELS));
	for (unsigned int i = 0; i < SPECTRUM_CHANNELS; ++i) {
		spectrum_cursor_right(&s);
	}
	assert(s.cursor == 12);
	s.cursor = (uint8_t)last;
	spectrum_cursor_right(&s);
	assert(s.cursor == 0);
}

static void test_peak_and_animation(void)
{
	struct spectrum s;
	spectrum_init(&s);
	fill(&s, 12, true);
	spectrum_animate(&s, 100, 100);
	assert(s.level[12] == 40U * SPECTRUM_ONE_PERCENT);
	assert(s.peak[12] == SPECTRUM_FULL_SCALE);
	fill(&s, 12, false);
	for (uint32_t now = 200; now <= 1000; now += 100) {
		spectrum_animate(&s, now, 100);
		assert(s.peak[12] == SPECTRUM_FULL_SCALE);
	}
	assert(s.level[12] == 0);
	spectrum_animate(&s, 1200, 200);
	assert(s.peak[12] == 95U * SPECTRUM_ONE_PERCENT);
	for (uint32_t now = 1400; now < 10000; now += 200) {
		spectrum_animate(&s, now, 200);
	}
	assert(s.peak[12] == 0); /* Saturates, never unsigned-underflows. */
	fill(&s, 12, true);
	spectrum_animate(&s, UINT32_MAX - 500U, 100);
	fill(&s, 12, false);
	spectrum_animate(&s, UINT32_MAX - 300U, 200);
	assert(s.peak[12] == SPECTRUM_FULL_SCALE);
	spectrum_animate(&s, 600, 200);
	assert(s.peak[12] < SPECTRUM_FULL_SCALE);
}

static void test_button(void)
{
	struct button_filter b;
	button_filter_init(&b, false, 0);
	assert(!button_filter_update(&b, true, 1));
	assert(!button_filter_update(&b, false, 11));
	assert(!button_filter_update(&b, true, 12));
	assert(!button_filter_update(&b, true, 41));
	assert(button_filter_update(&b, true, 42));
	assert(!button_filter_update(&b, true, 541));
	assert(button_filter_update(&b, true, 542)); /* First repeat after 500 ms. */
	assert(!button_filter_update(&b, true, 641));
	assert(button_filter_update(&b, true, 642));
	assert(button_filter_update(&b, true, 5000)); /* A delayed poll emits one event. */
	assert(!button_filter_update(&b, true, 5000)); /* Never a catch-up burst. */
	assert(!button_filter_update(&b, false, 5001));
	assert(!button_filter_update(&b, true, 5005)); /* Release bounce, no event. */
	assert(!button_filter_update(&b, false, 5010));
	assert(!button_filter_update(&b, false, 5040));
	assert(!button_filter_update(&b, true, 5041));
	assert(button_filter_update(&b, true, 5071));
	button_filter_init(&b, true, 0); /* Boot while held: must release first. */
	assert(!button_filter_update(&b, true, 1000));
	assert(!button_filter_update(&b, false, 1001));
	assert(!button_filter_update(&b, false, 1031));
	assert(!button_filter_update(&b, true, 1032));
	assert(button_filter_update(&b, true, 1062));
	button_filter_init(&b, false, UINT32_MAX - 50U);
	assert(!button_filter_update(&b, true, UINT32_MAX - 10U));
	assert(button_filter_update(&b, true, 20)); /* Debounce survives wrap. */
	button_filter_init(&b, false, UINT32_MAX - 300U);
	assert(!button_filter_update(&b, true, UINT32_MAX - 230U));
	assert(button_filter_update(&b, true, UINT32_MAX - 200U));
	assert(!button_filter_update(&b, true, 298));
	assert(button_filter_update(&b, true, 299)); /* Repeat deadline wraps too. */
	assert(!button_filter_update(&b, false, 399)); /* Release exactly when due. */
	assert(!button_filter_update(&b, false, 429));
	assert(!button_filter_update(&b, false, 1000));
}

int main(void)
{
	test_rolling_activity();
	test_peak_and_animation();
	test_button();
	puts("PASS: rolling samples, peak hold, cursor, debounce, long-press repeat, timer wrap");
	return 0;
}
