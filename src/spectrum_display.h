/* SPDX-License-Identifier: Apache-2.0 */
#ifndef APP_SPECTRUM_DISPLAY_H_
#define APP_SPECTRUM_DISPLAY_H_
#include "spectrum.h"
#include <zephyr/device.h>
int spectrum_display_init(const struct device *panel);
/* Snapshot the model for this frame, so later input cannot tear the cursor. */
void spectrum_display_begin(const struct spectrum *s, int radio_error, bool button_ok);
bool spectrum_display_busy(void);
/* Send at most four rows; scan and poll input between tiles. */
int spectrum_display_step(void);
#endif
