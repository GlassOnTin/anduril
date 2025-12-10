/*
 * sim_tiny1634.h - ATtiny1634 core header for simavr
 *
 * Copyright (C) 2024 Ian Dobbie
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef __SIM_TINY1634_H__
#define __SIM_TINY1634_H__

#include "sim_avr.h"
#include "avr_eeprom.h"
#include "avr_flash.h"
#include "avr_watchdog.h"
#include "avr_extint.h"
#include "avr_ioport.h"
#include "avr_timer.h"
#include "avr_adc.h"

void tiny1634_init(struct avr_t * avr);
void tiny1634_reset(struct avr_t * avr);

/*
 * ATtiny1634 MCU structure
 */
struct mcu_t {
    avr_t core;
    avr_eeprom_t eeprom;
    avr_flash_t selfprog;
    avr_watchdog_t watchdog;
    avr_extint_t extint;
    avr_ioport_t porta, portb, portc;
    avr_timer_t timer0, timer1;
    avr_adc_t adc;
};

#endif /* __SIM_TINY1634_H__ */
