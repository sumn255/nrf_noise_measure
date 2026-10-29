/* SPDX-License-Identifier: Apache-2.0 */
#ifndef APP_SI24R1_H_
#define APP_SI24R1_H_

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>

#define SI24R1_MAX_CHANNEL 125U
#define SI24R1_MIN_WINDOW_US 200U
#define SI24R1_MAX_WINDOW_US 5000U

/* Single-owner synchronous driver. Serialize externally before using in threads.
 * Not an upstream Zephyr networking device; no packet TX/RX API is implemented.
 */
struct si24r1 {
	struct spi_dt_spec spi;
	struct gpio_dt_spec ce;
	bool ready;
};

struct si24r1_snapshot {
	uint8_t status;
	uint8_t config;
	uint8_t en_aa;
	uint8_t en_rxaddr;
	uint8_t rf_setup;
	uint8_t channel;
};

/* Forces CE low, verifies writable registers, disables ACK and packet pipes,
 * then powers up in RX standby. On failure the caller must not sample.
 * This probes register compatibility, not a unique manufacturer/chip ID.
 */
int si24r1_init(struct si24r1 *radio);

/* One independent detection window. Frequency = 2400 + channel (MHz).
 * window_us includes RX settling; must be 200..5000 us.
 * On success CE is low and *detected contains register 0x09 bit 0.
 * On errors *detected is unchanged; attempt to leave CE low.
 */
int si24r1_sample(struct si24r1 *radio, uint8_t channel, uint32_t window_us,
		 bool *detected);
int si24r1_get_snapshot(struct si24r1 *radio, struct si24r1_snapshot *snapshot);
int si24r1_power_down(struct si24r1 *radio);

#endif
