/*
 * LCD panel property definitions.
 *
 * Copyright (c) 2012 Samsung Electronics
 *
 * Joongmock Shin <jmock.shin@samsung.com>
 * Eunchul Kim <chulspro.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#ifndef LCD_PROPERTY_H
#define LCD_PROPERTY_H

/* definition of flip */
enum lcd_property_flip {
	LCD_PROPERTY_FLIP_NONE = (0 << 0),
	LCD_PROPERTY_FLIP_VERTICAL = (1 << 0),
	LCD_PROPERTY_FLIP_HORIZONTAL = (1 << 1),
};

/*
 * A structure for lcd property.
 *
 * @flip: flip information for each lcd.
 * @esd_irq: LCD ESD detection irq.
 * @esd_level: esd level state.
 * @get_esd_level: get function of esd pin state.
 * @esd_gpio_suspend: suspend function of esd_gpio.
 * @esd_gpio_resume: resume function of esd_gpio.
 * @reset_low: LCD reset low function.
 */
struct lcd_property {
	enum lcd_property_flip flip;
	int esd_irq;
	int esd_level;
	int (*get_esd_level)(void);
	void (*esd_gpio_suspend)(void);
	void (*esd_gpio_resume)(void);
	void (*reset_low)(void);
};

#endif /* LCD_PROPERTY_H */

