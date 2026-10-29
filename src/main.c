/* SPDX-License-Identifier: Apache-2.0 */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "button_filter.h"
#include "si24r1.h"
#include "spectrum.h"
#include "spectrum_display.h"

LOG_MODULE_REGISTER(scanner, LOG_LEVEL_INF);
#define RADIO_NODE DT_NODELABEL(si24)
#define FRAME_INTERVAL_MS 100U
#define SCAN_BATCH 4U
BUILD_ASSERT(SPECTRUM_CHANNELS - 1U <= SI24R1_MAX_CHANNEL,
	     "Scan range exceeds the radio's supported channels");

static struct si24r1 radio = {
	.spi = SPI_DT_SPEC_GET(RADIO_NODE, SPI_WORD_SET(8) | SPI_TRANSFER_MSB, 0),
	.ce = GPIO_DT_SPEC_GET(RADIO_NODE, ce_gpios),
};
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec key = GPIO_DT_SPEC_GET(DT_ALIAS(scan_button), gpios);
static const struct device *const panel = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
static struct spectrum spectrum;
static struct button_filter key_filter;

int main(void)
{
	bool led_ok = gpio_is_ready_dt(&led);
	bool button_ok = gpio_is_ready_dt(&key);
	bool led_on = false;
	unsigned int channel = 0;
	unsigned int sweep_start = 0;
	uint32_t sweeps = 0;
	uint32_t frames = 0;
	uint32_t last_frame;
	uint32_t last_led;
	uint32_t last_log;
	int ret;

	printk("\nSI24 2.4 GHz activity scanner | Zephyr 4.2.0\n");
	LOG_INF("Console: USART0 TX=PB6 RX=PB7, 115200 8N1; button=PB8 to GND");
	spectrum_init(&spectrum);
	if (led_ok) {
		led_ok = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE) == 0;
	}
	if (button_ok) {
		button_ok = gpio_pin_configure_dt(&key, GPIO_INPUT) == 0;
	}
	if (button_ok) {
		ret = gpio_pin_get_dt(&key);
		button_ok = ret >= 0;
		if (button_ok) {
			button_filter_init(&key_filter, ret != 0, k_uptime_get_32());
		}
	}
	if (!button_ok) {
		LOG_ERR("Button GPIO init failed; scanning will continue");
	}
	int radio_error = si24r1_init(&radio);
	if (radio_error) {
		LOG_ERR("SI24 init failed: %d; check power/wiring and remove SD card", radio_error);
	} else {
		LOG_INF("Scanning %u..%u MHz (%u channels), %u windows/bin, %u us/window",
			SPECTRUM_FIRST_MHZ, SPECTRUM_LAST_MHZ, SPECTRUM_CHANNELS,
			SPECTRUM_WINDOW_SAMPLES, SI24R1_MIN_WINDOW_US);
	}
	ret = spectrum_display_init(panel);
	bool display_ok = ret == 0;
	if (!display_ok) {
		LOG_ERR("LCD init failed: %d", ret);
	}
	last_frame = last_led = last_log = k_uptime_get_32();
	if (display_ok) {
		spectrum_display_begin(&spectrum, radio_error, button_ok);
	}

	while (true) {
		uint32_t now = k_uptime_get_32();
		if (button_ok) {
			ret = gpio_pin_get_dt(&key);
			if (ret < 0) {
				LOG_ERR("Button GPIO read failed: %d", ret);
				button_ok = false;
			} else if (button_filter_update(&key_filter, ret != 0, now)) {
				spectrum_cursor_right(&spectrum);
				LOG_INF("Cursor: %u MHz", SPECTRUM_FIRST_MHZ + spectrum.cursor);
			}
		}
		if (!radio_error) {
			for (unsigned int i = 0; i < SCAN_BATCH; ++i) {
				bool hit;
				radio_error = si24r1_sample(&radio, (uint8_t)channel,
					SI24R1_MIN_WINDOW_US, &hit);
				if (radio_error) {
					LOG_ERR("Sample ch%u failed: %d", channel, radio_error);
					break;
				}
				spectrum_record(&spectrum, channel, hit);
				channel = (channel + 1U) % SPECTRUM_CHANNELS;
				if (channel == sweep_start) {
					sweeps++;
					/* Rotate start to reduce alignment with periodic signals. */
					sweep_start = (sweep_start + 1U) % SPECTRUM_CHANNELS;
					channel = sweep_start;
				}
			}
		}
		now = k_uptime_get_32();
		if ((uint32_t)(now - last_frame) >= FRAME_INTERVAL_MS &&
		    (!display_ok || !spectrum_display_busy())) {
			if (!radio_error) {
				spectrum_animate(&spectrum, now, now - last_frame);
			}
			last_frame = now;
			if (display_ok) {
				spectrum_display_begin(&spectrum, radio_error, button_ok);
			}
		}
		if (display_ok && spectrum_display_busy()) {
			ret = spectrum_display_step();
			if (ret < 0) {
				LOG_ERR("LCD write failed: %d", ret);
				display_ok = false;
			} else if (!spectrum_display_busy()) {
				frames++;
			}
		}
		if ((uint32_t)(now - last_led) >= 500U) {
			last_led = now;
			led_on = !led_on;
			if (led_ok) {
				led_ok = gpio_pin_set_dt(&led, led_on) == 0;
			}
		}
		if ((uint32_t)(now - last_log) >= 1000U) {
			uint32_t interval = now - last_log;
			last_log = now;
			if (!radio_error) {
				struct si24r1_snapshot snapshot;
				radio_error = si24r1_get_snapshot(&radio, &snapshot);
				if (radio_error) {
					LOG_ERR("SI24 health check failed: %d", radio_error);
				}
			}
			LOG_INF("%uMHz hits=%u/%u valid=%u activity=%u%% peak=%u%% "
				"sweeps/s=%u fps=%u err=%d LCD=%s KEY=%s",
				SPECTRUM_FIRST_MHZ + spectrum.cursor,
				spectrum.hits[spectrum.cursor], spectrum.samples[spectrum.cursor],
				spectrum_is_valid(&spectrum, spectrum.cursor),
				spectrum_percent(&spectrum, spectrum.cursor),
				spectrum.peak[spectrum.cursor] / SPECTRUM_ONE_PERCENT,
				sweeps * 1000U / interval, frames * 1000U / interval,
				radio_error, display_ok ? "ok" : "error", button_ok ? "ok" : "error");
			sweeps = frames = 0;
		}
		k_sleep(K_MSEC(1));
	}
	return 0;
}
