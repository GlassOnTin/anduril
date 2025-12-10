// sim-hwdef.h: Hardware stub definitions for D4K-3ch emulator
// Provides fake AVR registers and macros for PC-based simulation
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// =============================================================================
// AVR Register Stubs
// =============================================================================

// Port registers
extern volatile uint8_t sim_PORTA, sim_PORTB, sim_PORTC;
extern volatile uint8_t sim_DDRA, sim_DDRB, sim_DDRC;
extern volatile uint8_t sim_PINA, sim_PINB, sim_PINC;
extern volatile uint8_t sim_PUEA, sim_PUEB, sim_PUEC;

#define PORTA sim_PORTA
#define PORTB sim_PORTB
#define PORTC sim_PORTC
#define DDRA  sim_DDRA
#define DDRB  sim_DDRB
#define DDRC  sim_DDRC
#define PINA  sim_PINA
#define PINB  sim_PINB
#define PINC  sim_PINC
#define PUEA  sim_PUEA
#define PUEB  sim_PUEB
#define PUEC  sim_PUEC

// Timer0 registers (8-bit, used for main2 PWM)
extern volatile uint8_t sim_TCNT0, sim_OCR0A, sim_OCR0B;
extern volatile uint8_t sim_TCCR0A, sim_TCCR0B;

#define TCNT0  sim_TCNT0
#define OCR0A  sim_OCR0A
#define OCR0B  sim_OCR0B
#define TCCR0A sim_TCCR0A
#define TCCR0B sim_TCCR0B

// Timer1 registers (16-bit, used for led3/led4 PWM)
extern volatile uint16_t sim_TCNT1, sim_OCR1A, sim_OCR1B, sim_ICR1;
extern volatile uint8_t sim_TCCR1A, sim_TCCR1B;

#define TCNT1  sim_TCNT1
#define OCR1A  sim_OCR1A
#define OCR1B  sim_OCR1B
#define ICR1   sim_ICR1
#define TCCR1A sim_TCCR1A
#define TCCR1B sim_TCCR1B

// Timer interrupt mask
extern volatile uint8_t sim_TIMSK;
#define TIMSK sim_TIMSK

// ADC registers
extern volatile uint8_t sim_ADMUX, sim_ADCSRA, sim_ADCSRB;
extern volatile uint16_t sim_ADC_result;
#define ADMUX  sim_ADMUX
#define ADCSRA sim_ADCSRA
#define ADCSRB sim_ADCSRB
#define ADC    sim_ADC_result
#define ADCH   ((uint8_t)(sim_ADC_result >> 8))
#define ADCL   ((uint8_t)(sim_ADC_result & 0xFF))

// Pin change interrupt
extern volatile uint8_t sim_PCMSK0, sim_PCMSK1;
extern volatile uint8_t sim_GIMSK, sim_GIFR;
#define PCMSK0 sim_PCMSK0
#define PCMSK1 sim_PCMSK1
#define GIMSK  sim_GIMSK
#define GIFR   sim_GIFR

// Watchdog
extern volatile uint8_t sim_WDTCSR;
#define WDTCSR sim_WDTCSR

// Clock prescaler
extern volatile uint8_t sim_CLKPR;
#define CLKPR sim_CLKPR

// EEPROM (file-backed in simulator)
extern volatile uint8_t sim_EECR, sim_EEDR;
extern volatile uint16_t sim_EEAR;
#define EECR sim_EECR
#define EEDR sim_EEDR
#define EEAR sim_EEAR
#define EEARL ((uint8_t)(sim_EEAR & 0xFF))
#define EEARH ((uint8_t)(sim_EEAR >> 8))

// =============================================================================
// AVR Bit Definitions (from datasheet)
// =============================================================================

// Port A pins
#define PA0 0
#define PA1 1
#define PA2 2
#define PA3 3
#define PA4 4
#define PA5 5
#define PA6 6
#define PA7 7

// Port B pins
#define PB0 0
#define PB1 1
#define PB2 2
#define PB3 3

// Port C pins
#define PC0 0
#define PC1 1
#define PC2 2
#define PC3 3
#define PC4 4
#define PC5 5

// Timer0 control bits
#define WGM00  0
#define WGM01  1
#define WGM02  3
#define COM0A0 6
#define COM0A1 7
#define COM0B0 4
#define COM0B1 5
#define CS00   0
#define CS01   1
#define CS02   2

// Timer1 control bits
#define WGM10  0
#define WGM11  1
#define WGM12  3
#define WGM13  4
#define COM1A0 6
#define COM1A1 7
#define COM1B0 4
#define COM1B1 5
#define CS10   0
#define CS11   1
#define CS12   2

// Timer interrupt bits
#define TOIE0  0
#define TOIE1  2
#define OCIE0A 1
#define OCIE1A 3

// ADC bits
#define ADEN   7
#define ADSC   6
#define ADIE   3
#define ADPS0  0
#define ADPS1  1
#define ADPS2  2
#define REFS0  6
#define REFS1  7

// PCINT bits
#define PCIE0  4
#define PCIE1  5
#define PCINT7 7

// Watchdog bits
#define WDE    3
#define WDIE   6
#define WDCE   4

// EEPROM bits
#define EERE   0
#define EEPE   1
#define EEMPE  2

// Clock prescaler
#define CLKPCE 7

// =============================================================================
// PROGMEM / Flash Memory Stubs
// =============================================================================

// In simulator, PROGMEM data lives in regular RAM
#define PROGMEM
#define PGM_P const char *
#define pgm_read_byte(addr)  (*(const uint8_t *)(addr))
#define pgm_read_word(addr)  (*(const uint16_t *)(addr))
#define pgm_read_dword(addr) (*(const uint32_t *)(addr))

// =============================================================================
// Interrupt Stubs
// =============================================================================

// These are NOPs in the simulator - interrupts handled manually
#define cli()  do {} while(0)
#define sei()  do {} while(0)

// ISR macro - just defines a regular function
#define ISR(vect) void vect(void)
#define EMPTY_INTERRUPT(vect) void vect(void) {}

// Interrupt vector names (for ISR declarations)
#define WDT_vect        sim_isr_wdt
#define TIMER0_OVF_vect sim_isr_timer0_ovf
#define TIMER1_OVF_vect sim_isr_timer1_ovf
#define ADC_vect        sim_isr_adc
#define PCINT0_vect     sim_isr_pcint0

// =============================================================================
// Simulator Control
// =============================================================================

// Interrupt request flags (set by simulator, checked by firmware)
extern volatile uint8_t irq_wdt;
extern volatile uint8_t irq_adc;
extern volatile uint8_t irq_pcint;

// Simulated hardware state (read by renderer)
typedef struct {
    uint16_t main2_pwm;  // Green channel (2 LEDs) - 15-bit DSM
    uint16_t led3_pwm;   // Red channel - 15-bit DSM
    uint16_t led4_pwm;   // Blue channel - 15-bit DSM
    uint8_t  aux_r;      // Aux LED red (0 or 1)
    uint8_t  aux_g;      // Aux LED green (0 or 1)
    uint8_t  aux_b;      // Aux LED blue (0 or 1)
    uint8_t  button_led; // Button LED (0 or 1)
    bool     button_pressed;
    uint8_t  voltage;    // Simulated battery voltage (0-255 = 0-5.1V)
    int16_t  temperature; // Simulated temperature (Kelvin << 6)
} sim_hw_state_t;

extern sim_hw_state_t sim_hw;

// Initialize simulator hardware state
void sim_hw_init(void);

// Simulate button press/release
void sim_button_press(void);
void sim_button_release(void);

// Get LED output levels for rendering (0-255 each)
void sim_get_rgb(uint8_t *r, uint8_t *g, uint8_t *b);

// =============================================================================
// MCU-specific stubs
// =============================================================================

#define F_CPU 8000000UL
#define BOGOMIPS (F_CPU/4000)
#define PROGMEM_SIZE 16384
#define EEPROM_SIZE 256

// Inline functions that would normally be in arch/attiny1634.h
static inline void mcu_clock_speed(void) {}
static inline void clock_prescale_set(uint8_t n) { (void)n; }
static inline void mcu_wdt_active(void) {}
static inline void mcu_wdt_standby(void) {}
static inline void mcu_wdt_stop(void) {}
static inline void mcu_pcint_on(void) {}
static inline void mcu_pcint_off(void) {}
static inline void mcu_adc_sleep_mode(void) {}
static inline void mcu_adc_start_measurement(void) {}
static inline void mcu_adc_off(void) {}
static inline uint16_t mcu_adc_result(void) { return sim_ADC_result; }
static inline uint8_t mcu_adc_lsb(void) { return sim_ADC_result & 0xFF; }
static inline void mcu_set_admux_therm(void) { sim_ADMUX = 0b10001110; }
static inline void mcu_set_admux_voltage(void) { sim_ADMUX = 0b00001101; }

// Voltage conversion (Volts * 50, range 0-255 = 0-5.1V)
static inline uint8_t mcu_vdd_raw2cooked(uint16_t measurement) {
    // Simplified - just return simulated voltage
    return sim_hw.voltage;
}

static inline uint8_t mcu_vdivider_raw2cooked(uint16_t measurement) {
    return sim_hw.voltage;
}

// Temperature conversion (Kelvin << 6)
static inline uint16_t mcu_temp_raw2cooked(uint16_t measurement) {
    return sim_hw.temperature;
}

// Reboot (just exits in simulator)
static inline void reboot(void) {
    // In real hardware this resets the MCU
    // In simulator we could restart or just continue
}

static inline void prevent_reboot_loop(void) {
    // NOP in simulator
}

// =============================================================================
// Additional macros used by Anduril
// =============================================================================

#define ADMUX_VCC    0b00001101
#define ADMUX_THERM  0b10001110
#define V_REF        REFS1

#define hwdef_set_admux_therm   mcu_set_admux_therm
#define hwdef_set_admux_voltage mcu_set_admux_voltage
#define voltage_raw2cooked      mcu_vdd_raw2cooked
#define temp_raw2cooked         mcu_temp_raw2cooked

#define mcu_wdt_vect_clear()
#define mcu_adc_vect_clear()
#define mcu_switch_vect_clear()
