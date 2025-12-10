// sim-hwdef.c: Hardware stub implementations for D4K-3ch emulator
#include "sim-hwdef.h"

// =============================================================================
// Port Registers
// =============================================================================

volatile uint8_t sim_PORTA = 0, sim_PORTB = 0, sim_PORTC = 0;
volatile uint8_t sim_DDRA = 0, sim_DDRB = 0, sim_DDRC = 0;
volatile uint8_t sim_PINA = 0xFF, sim_PINB = 0xFF, sim_PINC = 0xFF;  // Pull-ups default high
volatile uint8_t sim_PUEA = 0, sim_PUEB = 0, sim_PUEC = 0;

// =============================================================================
// Timer Registers
// =============================================================================

// Timer0 (8-bit)
volatile uint8_t sim_TCNT0 = 0, sim_OCR0A = 0, sim_OCR0B = 0;
volatile uint8_t sim_TCCR0A = 0, sim_TCCR0B = 0;

// Timer1 (16-bit)
volatile uint16_t sim_TCNT1 = 0, sim_OCR1A = 0, sim_OCR1B = 0, sim_ICR1 = 255;
volatile uint8_t sim_TCCR1A = 0, sim_TCCR1B = 0;

// Timer interrupt mask
volatile uint8_t sim_TIMSK = 0;

// =============================================================================
// ADC Registers
// =============================================================================

volatile uint8_t sim_ADMUX = 0, sim_ADCSRA = 0, sim_ADCSRB = 0;
volatile uint16_t sim_ADC_result = 512;  // Mid-range default

// =============================================================================
// Pin Change Interrupt Registers
// =============================================================================

volatile uint8_t sim_PCMSK0 = 0, sim_PCMSK1 = 0;
volatile uint8_t sim_GIMSK = 0, sim_GIFR = 0;

// =============================================================================
// Other Registers
// =============================================================================

volatile uint8_t sim_WDTCSR = 0;
volatile uint8_t sim_CLKPR = 0;
volatile uint8_t sim_EECR = 0, sim_EEDR = 0;
volatile uint16_t sim_EEAR = 0;

// =============================================================================
// Interrupt Request Flags
// =============================================================================

volatile uint8_t irq_wdt = 0;
volatile uint8_t irq_adc = 0;
volatile uint8_t irq_pcint = 0;

// =============================================================================
// Simulated Hardware State
// =============================================================================

sim_hw_state_t sim_hw = {
    .main2_pwm = 0,
    .led3_pwm = 0,
    .led4_pwm = 0,
    .aux_r = 0,
    .aux_g = 0,
    .aux_b = 0,
    .button_led = 0,
    .button_pressed = false,
    .voltage = 170,      // ~3.4V default (170 * 0.02 = 3.4V)
    .temperature = 19200 // ~300K (room temp) << 6
};

// =============================================================================
// Simulator Control Functions
// =============================================================================

void sim_hw_init(void) {
    // Reset all registers to defaults
    sim_PORTA = sim_PORTB = sim_PORTC = 0;
    sim_DDRA = sim_DDRB = sim_DDRC = 0;
    sim_PINA = sim_PINB = sim_PINC = 0xFF;  // Inputs read high (pull-ups)

    sim_TCNT0 = sim_OCR0A = sim_OCR0B = 0;
    sim_TCNT1 = sim_OCR1A = sim_OCR1B = 0;
    sim_ICR1 = 255;

    sim_hw.main2_pwm = 0;
    sim_hw.led3_pwm = 0;
    sim_hw.led4_pwm = 0;
    sim_hw.aux_r = sim_hw.aux_g = sim_hw.aux_b = 0;
    sim_hw.button_led = 0;
    sim_hw.button_pressed = false;

    irq_wdt = irq_adc = irq_pcint = 0;
}

void sim_button_press(void) {
    sim_hw.button_pressed = true;
    // Button is on PA7, active low
    sim_PINA &= ~(1 << PA7);
    // Trigger pin change interrupt
    irq_pcint = 1;
}

void sim_button_release(void) {
    sim_hw.button_pressed = false;
    // Button released - PA7 goes high
    sim_PINA |= (1 << PA7);
    // Trigger pin change interrupt
    irq_pcint = 1;
}

// Update simulated hardware state from register values
// Called after each tick to sync state for rendering
void sim_hw_update(void) {
    // Main2 (green) PWM - stored in OCR0A (8-bit only in hardware)
    // But the firmware uses DSM for higher resolution
    // We read the DSM level variables directly

    // Aux LEDs - direct port control (PA5=R, PA4=G, PA3=B)
    sim_hw.aux_r = (sim_PORTA & (1 << PA5)) ? 1 : 0;
    sim_hw.aux_g = (sim_PORTA & (1 << PA4)) ? 1 : 0;
    sim_hw.aux_b = (sim_PORTA & (1 << PA3)) ? 1 : 0;

    // Button LED - PA2
    sim_hw.button_led = (sim_PORTA & (1 << PA2)) ? 1 : 0;
}

// Get RGB output for rendering (converts 15-bit DSM to 8-bit)
void sim_get_rgb(uint8_t *r, uint8_t *g, uint8_t *b) {
    // Scale 15-bit DSM values (0-32767) to 8-bit (0-255)
    // led3 = Red, main2 = Green (x2 LEDs), led4 = Blue
    *r = (uint8_t)(sim_hw.led3_pwm >> 7);   // Red
    *g = (uint8_t)(sim_hw.main2_pwm >> 7);  // Green (already scaled for 2 LEDs)
    *b = (uint8_t)(sim_hw.led4_pwm >> 7);   // Blue
}
