/* SPDX-License-Identifier: Apache-2.0 */
#include "si24r1.h"

#include <errno.h>
#include <stddef.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(si24r1, LOG_LEVEL_INF);

#define REG_CONFIG       0x00
#define REG_EN_AA        0x01
#define REG_EN_RXADDR    0x02
#define REG_SETUP_AW     0x03
#define REG_SETUP_RETR   0x04
#define REG_RF_CH        0x05
#define REG_RF_SETUP     0x06
#define REG_STATUS       0x07
#define REG_DETECT       0x09

#define CMD_WRITE_REG    0x20
#define CMD_FLUSH_TX     0xe1
#define CMD_FLUSH_RX     0xe2
#define CMD_NOP          0xff

/* Mask IRQs, enable two-byte CRC, select RX; PWR_UP is added separately. */
#define CONFIG_RX_OFF    0x7d
#define CONFIG_PWR_UP    BIT(1)
#define CONFIG_RX_ON     (CONFIG_RX_OFF | CONFIG_PWR_UP)
#define RF_SETUP_2MBPS   BIT(3)

static int transfer(struct si24r1 *radio, const uint8_t *tx, uint8_t *rx, size_t len)
{
	const struct spi_buf tx_buf = {.buf = (void *)tx, .len = len};
	const struct spi_buf rx_buf = {.buf = rx, .len = len};
	const struct spi_buf_set tx_set = {.buffers = &tx_buf, .count = 1};
	const struct spi_buf_set rx_set = {.buffers = &rx_buf, .count = 1};

	/* One transaction keeps CSN low for command AND data. */
	return spi_transceive_dt(&radio->spi, &tx_set, &rx_set);
}

static int read_reg(struct si24r1 *radio, uint8_t reg, uint8_t *value)
{
	const uint8_t tx[] = {reg & 0x1fU, CMD_NOP};
	uint8_t rx[2];
	int ret = transfer(radio, tx, rx, sizeof(tx));

	if (ret == 0) {
		*value = rx[1];
	}
	return ret;
}

static int command(struct si24r1 *radio, uint8_t cmd)
{
	uint8_t rx;

	return transfer(radio, &cmd, &rx, 1);
}

static int write_reg(struct si24r1 *radio, uint8_t reg, uint8_t value)
{
	const uint8_t tx[] = {CMD_WRITE_REG | (reg & 0x1fU), value};
	uint8_t rx[2];

	return transfer(radio, tx, rx, sizeof(tx));
}

static int write_checked(struct si24r1 *radio, uint8_t reg, uint8_t value)
{
	uint8_t actual;
	int ret = write_reg(radio, reg, value);

	if (ret == 0) {
		ret = read_reg(radio, reg, &actual);
	}
	if (ret == 0 && actual != value) {
		LOG_ERR("Register 0x%02x: wrote 0x%02x, read 0x%02x",
			reg, value, actual);
		return -EIO;
	}
	return ret;
}

int si24r1_init(struct si24r1 *radio)
{
	uint8_t original_channel;
	uint8_t status;
	int ret;
	const struct {
		uint8_t reg;
		uint8_t value;
	} settings[] = {
		{REG_CONFIG, CONFIG_RX_OFF},
		{REG_EN_AA, 0x00},        /* Never transmit automatic acknowledgments. */
		{REG_EN_RXADDR, 0x00},    /* No packet matching/FIFO traffic needed. */
		{REG_SETUP_AW, 0x03},
		{REG_SETUP_RETR, 0x00},
		{REG_RF_SETUP, RF_SETUP_2MBPS},
		{REG_RF_CH, 12},
	};

	if (radio == NULL) {
		return -EINVAL;
	}
	radio->ready = false;
	if (!gpio_is_ready_dt(&radio->ce)) {
		return -ENODEV;
	}
	ret = gpio_pin_configure_dt(&radio->ce, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		return ret;
	}
	if (!spi_is_ready_dt(&radio->spi)) {
		return -ENODEV;
	}
	k_msleep(5);
	ret = read_reg(radio, REG_STATUS, &status);
	if (ret < 0) {
		return ret;
	}
	LOG_INF("Probe STATUS=0x%02x (not a chip ID)", status);
	if (status == 0xff || status == 0x00) {
		return -ENODEV;
	}
	/* CE remains low throughout probing, even if a previous MCU run left
	 * the radio powered up. Restore the channel after the two-pattern test.
	 */
	ret = read_reg(radio, REG_RF_CH, &original_channel);
	if (ret < 0) {
		return ret;
	}
	ret = write_checked(radio, REG_RF_CH, 0x2a);
	if (ret == 0) {
		ret = write_checked(radio, REG_RF_CH, 0x55);
	}
	int restore_ret = write_reg(radio, REG_RF_CH, original_channel);

	if (ret < 0 || restore_ret < 0) {
		return ret < 0 ? ret : restore_ret;
	}
	for (size_t i = 0; i < ARRAY_SIZE(settings); ++i) {
		ret = write_checked(radio, settings[i].reg, settings[i].value);
		if (ret < 0) {
			goto fail;
		}
	}
	ret = command(radio, CMD_FLUSH_RX);
	if (ret == 0) {
		ret = command(radio, CMD_FLUSH_TX);
	}
	if (ret == 0) {
		ret = write_reg(radio, REG_STATUS, 0x70); /* Clear pending IRQ flags. */
	}
	if (ret == 0) {
		ret = write_checked(radio, REG_CONFIG, CONFIG_RX_ON);
	}
	if (ret < 0) {
		goto fail;
	}
	k_msleep(5);
	radio->ready = true;
	LOG_INF("Register self-test passed; RX only, ACK off, SPI %u Hz",
		radio->spi.config.frequency);
	return 0;

fail:
	(void)gpio_pin_set_dt(&radio->ce, 0);
	(void)write_reg(radio, REG_CONFIG, CONFIG_RX_OFF);
	return ret;
}

int si24r1_sample(struct si24r1 *radio, uint8_t channel, uint32_t window_us,
		 bool *detected)
{
	uint8_t value;
	int ret;

	if (radio == NULL || detected == NULL || channel > SI24R1_MAX_CHANNEL ||
	    window_us < SI24R1_MIN_WINDOW_US || window_us > SI24R1_MAX_WINDOW_US) {
		return -EINVAL;
	}
	if (!radio->ready) {
		return -EACCES;
	}
	ret = gpio_pin_set_dt(&radio->ce, 0);
	if (ret < 0) {
		goto fail;
	}
	k_busy_wait(5);
	ret = write_checked(radio, REG_RF_CH, channel);
	if (ret < 0) {
		goto fail;
	}
	ret = gpio_pin_set_dt(&radio->ce, 1);
	if (ret < 0) {
		goto fail;
	}
	/* New CE rising edge starts a new detection window; do not repeatedly
	 * count the same latched bit as independent observations.
	 */
	k_busy_wait(window_us);
	ret = gpio_pin_set_dt(&radio->ce, 0);
	if (ret < 0) {
		goto fail;
	}
	ret = read_reg(radio, REG_DETECT, &value);
	if (ret < 0) {
		goto fail;
	}
	if (value & ~BIT(0)) {
		ret = -EIO; /* Detect e.g. a disconnected MISO reading 0xff. */
		goto fail;
	}
	*detected = (value & BIT(0)) != 0;
	return 0;

fail:
	(void)gpio_pin_set_dt(&radio->ce, 0);
	radio->ready = false;
	return ret;
}

int si24r1_get_snapshot(struct si24r1 *radio, struct si24r1_snapshot *snapshot)
{
	struct si24r1_snapshot result;
	int ret;

	if (radio == NULL || snapshot == NULL) {
		return -EINVAL;
	}
	if (!radio->ready) {
		return -EACCES;
	}
	uint8_t *const outputs[] = {&result.status, &result.config, &result.en_aa,
		&result.en_rxaddr, &result.rf_setup, &result.channel};
	const uint8_t regs[] = {REG_STATUS, REG_CONFIG, REG_EN_AA,
		REG_EN_RXADDR, REG_RF_SETUP, REG_RF_CH};

	for (size_t i = 0; i < ARRAY_SIZE(regs); ++i) {
		ret = read_reg(radio, regs[i], outputs[i]);
		if (ret < 0) {
			return ret;
		}
	}
	/* A successful SPI transfer alone does not prove that the chip replied. */
	if (result.status == 0xff || result.config != CONFIG_RX_ON ||
	    result.en_aa != 0 || result.en_rxaddr != 0 ||
	    result.rf_setup != RF_SETUP_2MBPS || result.channel > SI24R1_MAX_CHANNEL) {
		return -EIO;
	}
	*snapshot = result;
	return 0;
}

int si24r1_power_down(struct si24r1 *radio)
{
	if (radio == NULL) {
		return -EINVAL;
	}
	if (!radio->ready) {
		return -EACCES;
	}
	int ce_ret = gpio_pin_set_dt(&radio->ce, 0);
	int ret = write_reg(radio, REG_CONFIG, CONFIG_RX_OFF);

	radio->ready = false;
	return ce_ret < 0 ? ce_ret : ret;
}
