/* SPDX-License-Identifier: Apache-2.0 */
#ifndef APP_BUTTON_FILTER_H_
#define APP_BUTTON_FILTER_H_
#include <stdbool.h>
#include <stdint.h>
#define BUTTON_DEBOUNCE_MS 30U
#define BUTTON_REPEAT_DELAY_MS 500U
#define BUTTON_REPEAT_INTERVAL_MS 100U
struct button_filter {
	bool candidate;
	bool stable;
	bool armed;
	bool repeat_enabled;
	uint32_t changed_ms;
	uint32_t next_repeat_ms;
};
void button_filter_init(struct button_filter *b, bool pressed, uint32_t now_ms);
bool button_filter_update(struct button_filter *b, bool pressed, uint32_t now_ms);
#endif
