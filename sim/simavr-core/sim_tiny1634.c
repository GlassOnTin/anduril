/*
 * sim_tiny1634.c - ATtiny1634 core for simavr
 *
 * Copyright (C) 2024 Ian Dobbie
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * ATtiny1634 specifications:
 * - 16KB Flash
 * - 1KB SRAM
 * - 256B EEPROM
 * - 3 GPIO ports (A: 8 pins, B: 4 pins, C: 6 pins)
 * - Timer0: 8-bit with PWM
 * - Timer1: 16-bit with PWM
 * - 12-channel 10-bit ADC
 * - WDT, 2x USART, USI
 */

#ifndef __concat
#define __concat(a, b) a ## b
#endif

#ifndef _BV
#define _BV(x) (1 << x)
#endif

#define SIM_VECTOR_SIZE    2
#define SIM_MMCU           "attiny1634"
#define SIM_CORENAME       mcu_tiny1634

#define _AVR_IO_H_
#define __ASSEMBLER__
#include "avr/iotn1634.h"

#include "sim_avr.h"
#include "sim_core_declare.h"
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
 * ATtiny1634 has an interesting PCINT setup:
 * - PCINT0-7 on PORTA (PA0-PA7)
 * - PCINT8-11 on PORTB (PB0-PB3)
 * - PCINT12-17 on PORTC (PC0-PC5)
 */

static const avr_extint_t tiny1634_extint = {
    .eint = {
        AVR_EXT_INT(INT0, INT0_vect),
    }
};

static const avr_ioport_t tiny1634_porta = {
    .name = 'A',
    .r_port = PORTA,
    .r_ddr = DDRA,
    .r_pin = PINA,

    .pcint = {
        .enable = AVR_IO_REGBIT(GIMSK, PCIE0),
        .raised = AVR_IO_REGBIT(GIFR, PCIF0),
        .vector = PCINT0_vect,
    },
    .r_pcint = PCMSK0,
};

static const avr_ioport_t tiny1634_portb = {
    .name = 'B',
    .r_port = PORTB,
    .r_ddr = DDRB,
    .r_pin = PINB,

    .pcint = {
        .enable = AVR_IO_REGBIT(GIMSK, PCIE1),
        .raised = AVR_IO_REGBIT(GIFR, PCIF1),
        .vector = PCINT1_vect,
    },
    .r_pcint = PCMSK1,
};

static const avr_ioport_t tiny1634_portc = {
    .name = 'C',
    .r_port = PORTC,
    .r_ddr = DDRC,
    .r_pin = PINC,

    .pcint = {
        .enable = AVR_IO_REGBIT(GIMSK, PCIE2),
        .raised = AVR_IO_REGBIT(GIFR, PCIF2),
        .vector = PCINT2_vect,
    },
    .r_pcint = PCMSK2,
};

static const avr_watchdog_t tiny1634_watchdog = {
    .enable = AVR_IO_REGBIT(WDTCSR, WDE),
    .watchdog = AVR_IO_REGBIT(WDTCSR, WDIE),
    .raised = AVR_IO_REGBIT(WDTCSR, WDIF),
    .vector = WDT_vect,
};

static const avr_eeprom_t tiny1634_eeprom = {
    .size = E2END + 1,  // 256 bytes
    .r_eearl = EEARL,
    .r_eearh = EEARH,
    .r_eedr = EEDR,
    .r_eecr = EECR,
    .ready = AVR_IO_REGBIT(EECR, EEPE),
    .enable = AVR_IO_REGBIT(EECR, EEMPE),
};

static const avr_flash_t tiny1634_flash = {
    .r_spm = SPMCSR,
    .spm_pagesize = SPM_PAGESIZE,
    .selfprgen = AVR_IO_REGBIT(SPMCSR, SPMEN),
};

/*
 * Timer0 - 8-bit with PWM on OC0A (PC0) and OC0B (PA5)
 */
static const avr_timer_t tiny1634_timer0 = {
    .name = '0',
    .disabled = AVR_IO_REGBIT(PRR, PRTIM0),
    .wgm = { AVR_IO_REGBIT(TCCR0A, WGM00), AVR_IO_REGBIT(TCCR0A, WGM01),
             AVR_IO_REGBIT(TCCR0B, WGM02) },
    .wgm_op = {
        [0] = AVR_TIMER_WGM_NORMAL8(),
        [1] = AVR_TIMER_WGM_PCPWM8(),
        [2] = AVR_TIMER_WGM_CTC(),
        [3] = AVR_TIMER_WGM_FASTPWM8(),
        [5] = AVR_TIMER_WGM_PCPWM(),
        [7] = AVR_TIMER_WGM_FASTPWM(),
    },
    .cs = { AVR_IO_REGBIT(TCCR0B, CS00), AVR_IO_REGBIT(TCCR0B, CS01),
            AVR_IO_REGBIT(TCCR0B, CS02) },
    .cs_div = { 0, 0, 3, 6, 8, 10 },  // /1, /8, /64, /256, /1024

    .r_tcnt = TCNT0,

    .overflow = {
        .enable = AVR_IO_REGBIT(TIMSK, TOIE0),
        .raised = AVR_IO_REGBIT(TIFR, TOV0),
        .vector = TIM0_OVF_vect,
    },
    .comp = {
        [AVR_TIMER_COMPA] = {
            .r_ocr = OCR0A,
            .com = AVR_IO_REGBITS(TCCR0A, COM0A0, 0x3),
            .com_pin = AVR_IO_REGBIT(PORTC, 0),  // OC0A on PC0
            .interrupt = {
                .enable = AVR_IO_REGBIT(TIMSK, OCIE0A),
                .raised = AVR_IO_REGBIT(TIFR, OCF0A),
                .vector = TIM0_COMPA_vect,
            },
        },
        [AVR_TIMER_COMPB] = {
            .r_ocr = OCR0B,
            .com = AVR_IO_REGBITS(TCCR0A, COM0B0, 0x3),
            .com_pin = AVR_IO_REGBIT(PORTA, 5),  // OC0B on PA5
            .interrupt = {
                .enable = AVR_IO_REGBIT(TIMSK, OCIE0B),
                .raised = AVR_IO_REGBIT(TIFR, OCF0B),
                .vector = TIM0_COMPB_vect,
            },
        },
    },
};

/*
 * Timer1 - 16-bit with PWM on OC1A (PB3) and OC1B (PA6)
 */
static const avr_timer_t tiny1634_timer1 = {
    .name = '1',
    .disabled = AVR_IO_REGBIT(PRR, PRTIM1),
    .wgm = { AVR_IO_REGBIT(TCCR1A, WGM10), AVR_IO_REGBIT(TCCR1A, WGM11),
             AVR_IO_REGBIT(TCCR1B, WGM12), AVR_IO_REGBIT(TCCR1B, WGM13) },
    .wgm_op = {
        [0]  = AVR_TIMER_WGM_NORMAL16(),
        [1]  = AVR_TIMER_WGM_PCPWM8(),
        [2]  = AVR_TIMER_WGM_PCPWM9(),
        [3]  = AVR_TIMER_WGM_PCPWM10(),
        [4]  = AVR_TIMER_WGM_CTC(),
        [5]  = AVR_TIMER_WGM_FASTPWM8(),
        [6]  = AVR_TIMER_WGM_FASTPWM9(),
        [7]  = AVR_TIMER_WGM_FASTPWM10(),
        [8]  = AVR_TIMER_WGM_ICPWM(),
        [9]  = AVR_TIMER_WGM_OCPWM(),
        [10] = AVR_TIMER_WGM_ICPWM(),
        [11] = AVR_TIMER_WGM_OCPWM(),
        [12] = AVR_TIMER_WGM_ICCTC(),
        [14] = AVR_TIMER_WGM_ICFASTPWM(),
        [15] = AVR_TIMER_WGM_OCFASTPWM(),
    },
    .cs = { AVR_IO_REGBIT(TCCR1B, CS10), AVR_IO_REGBIT(TCCR1B, CS11),
            AVR_IO_REGBIT(TCCR1B, CS12) },
    .cs_div = { 0, 0, 3, 6, 8, 10 },

    .r_tcnt = TCNT1L,
    .r_tcnth = TCNT1H,
    .r_icr = ICR1L,
    .r_icrh = ICR1H,

    .overflow = {
        .enable = AVR_IO_REGBIT(TIMSK, TOIE1),
        .raised = AVR_IO_REGBIT(TIFR, TOV1),
        .vector = TIM1_OVF_vect,
    },
    .icr = {
        .enable = AVR_IO_REGBIT(TIMSK, ICIE1),
        .raised = AVR_IO_REGBIT(TIFR, ICF1),
        .vector = TIM1_CAPT_vect,
    },
    .comp = {
        [AVR_TIMER_COMPA] = {
            .r_ocr = OCR1AL,
            .r_ocrh = OCR1AH,
            .com = AVR_IO_REGBITS(TCCR1A, COM1A0, 0x3),
            .com_pin = AVR_IO_REGBIT(PORTB, 3),  // OC1A on PB3
            .interrupt = {
                .enable = AVR_IO_REGBIT(TIMSK, OCIE1A),
                .raised = AVR_IO_REGBIT(TIFR, OCF1A),
                .vector = TIM1_COMPA_vect,
            },
        },
        [AVR_TIMER_COMPB] = {
            .r_ocr = OCR1BL,
            .r_ocrh = OCR1BH,
            .com = AVR_IO_REGBITS(TCCR1A, COM1B0, 0x3),
            .com_pin = AVR_IO_REGBIT(PORTA, 6),  // OC1B on PA6
            .interrupt = {
                .enable = AVR_IO_REGBIT(TIMSK, OCIE1B),
                .raised = AVR_IO_REGBIT(TIFR, OCF1B),
                .vector = TIM1_COMPB_vect,
            },
        },
    },
};

/*
 * ADC - 12 channels, 10-bit
 */
static const avr_adc_t tiny1634_adc = {
    .r_admux = ADMUX,
    .mux = { AVR_IO_REGBIT(ADMUX, MUX0), AVR_IO_REGBIT(ADMUX, MUX1),
             AVR_IO_REGBIT(ADMUX, MUX2), AVR_IO_REGBIT(ADMUX, MUX3) },
    .ref = { AVR_IO_REGBIT(ADMUX, REFS0), AVR_IO_REGBIT(ADMUX, REFS1) },
    .ref_values = { [0] = ADC_VREF_VCC, [1] = ADC_VREF_V110 },

    .adlar = AVR_IO_REGBIT(ADCSRB, ADLAR),
    .r_adcsra = ADCSRA,
    .aden = AVR_IO_REGBIT(ADCSRA, ADEN),
    .adsc = AVR_IO_REGBIT(ADCSRA, ADSC),
    .adate = AVR_IO_REGBIT(ADCSRA, ADATE),
    .adps = { AVR_IO_REGBIT(ADCSRA, ADPS0), AVR_IO_REGBIT(ADCSRA, ADPS1),
              AVR_IO_REGBIT(ADCSRA, ADPS2) },

    .r_adch = ADCH,
    .r_adcl = ADCL,

    .r_adcsrb = ADCSRB,
    .adts = { AVR_IO_REGBIT(ADCSRB, ADTS0), AVR_IO_REGBIT(ADCSRB, ADTS1),
              AVR_IO_REGBIT(ADCSRB, ADTS2) },
    .adts_op = {
        [0] = avr_adts_free_running,
        [1] = avr_adts_analog_comparator_0,
        [2] = avr_adts_external_interrupt_0,
        [3] = avr_adts_timer_0_compare_match_a,
        [4] = avr_adts_timer_0_overflow,
        [5] = avr_adts_timer_1_compare_match_b,
        [6] = avr_adts_timer_1_overflow,
        [7] = avr_adts_timer_1_capture_event,
    },

    .adc = {
        .enable = AVR_IO_REGBIT(ADCSRA, ADIE),
        .raised = AVR_IO_REGBIT(ADCSRA, ADIF),
        .vector = ADC_vect,
    },
    .muxmode = {
        [0] = AVR_ADC_SINGLE(0), [1] = AVR_ADC_SINGLE(1),
        [2] = AVR_ADC_SINGLE(2), [3] = AVR_ADC_SINGLE(3),
        [4] = AVR_ADC_SINGLE(4), [5] = AVR_ADC_SINGLE(5),
        [6] = AVR_ADC_SINGLE(6), [7] = AVR_ADC_SINGLE(7),
        [8] = AVR_ADC_SINGLE(8), [9] = AVR_ADC_SINGLE(9),
        [10] = AVR_ADC_SINGLE(10), [11] = AVR_ADC_SINGLE(11),
        [12] = AVR_ADC_REF(0),      // GND
        [13] = AVR_ADC_REF(1100),   // 1.1V internal ref
        [14] = AVR_ADC_TEMP(),      // Temperature sensor
    },
};

static avr_t * make(void)
{
    return avr_core_allocate(&SIM_CORENAME.core, sizeof(struct mcu_t));
}

avr_kind_t tiny1634 = {
    .names = { "attiny1634" },
    .make = make
};

AVR_MCU_DECLARE(tiny1634, SIM_CORENAME);

void tiny1634_init(struct avr_t * avr)
{
    struct mcu_t * mcu = (struct mcu_t*)avr;

    avr_eeprom_init(avr, &mcu->eeprom);
    avr_flash_init(avr, &mcu->selfprog);
    avr_watchdog_init(avr, &mcu->watchdog);
    avr_extint_init(avr, &mcu->extint);
    avr_ioport_init(avr, &mcu->porta);
    avr_ioport_init(avr, &mcu->portb);
    avr_ioport_init(avr, &mcu->portc);
    avr_timer_init(avr, &mcu->timer0);
    avr_timer_init(avr, &mcu->timer1);
    avr_adc_init(avr, &mcu->adc);
}

void tiny1634_reset(struct avr_t * avr)
{
    // Nothing special needed
}
