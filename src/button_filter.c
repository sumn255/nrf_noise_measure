/* SPDX-License-Identifier: Apache-2.0 */
#include "button_filter.h"
void button_filter_init(struct button_filter *b, bool pressed, uint32_t now_ms)
{
	b->candidate = pressed;
	b->stable = pressed;
	b->armed = !pressed;
	b->repeat_enabled = false;
	b->changed_ms = now_ms;
	b->next_repeat_ms = now_ms;
}
bool button_filter_update(struct button_filter *b, bool pressed, uint32_t now_ms)
{
	if (pressed != b->candidate) {
		b->candidate = pressed;
		b->changed_ms = now_ms;
	}
	if (b->candidate != b->stable) {
		if ((uint32_t)(now_ms - b->changed_ms) < BUTTON_DEBOUNCE_MS) {
			return false;
		}
		b->stable = b->candidate;
		if (!b->stable) {
			b->armed = true;
			b->repeat_enabled = false;
			return false;
		}
		if (b->armed) {
			b->armed = false;
			b->repeat_enabled = true;
			b->next_repeat_ms = now_ms + BUTTON_REPEAT_DELAY_MS;
			return true;
		}
	}
	/* Suppress repeats immediately on release, even before release debounce.
	 * A held key at boot never enables repeating until released and pressed.
	 */
	if (pressed && b->stable && b->repeat_enabled &&
	    (int32_t)(now_ms - b->next_repeat_ms) >= 0) {
		/* No catch-up burst if scanning or rendering delayed a poll. */
		b->next_repeat_ms = now_ms + BUTTON_REPEAT_INTERVAL_MS;
		return true;
	}
	return false;
}
