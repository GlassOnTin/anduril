// sim-test-harness.c: Test harness for real D4K-3ch channel mode calculations
// Copyright (C) 2024 Ian Dobbie / ToyKeeper
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file extracts and tests the REAL channel mode calculations from
// the D4K-3ch firmware, without needing to compile the entire Anduril firmware.
// It tests: CCT mode, HSV mode, and balanced white mode.

#define _DEFAULT_SOURCE  // For usleep()

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>

#include "sim-render.h"

// =============================================================================
// Type definitions to match firmware
// =============================================================================

typedef uint16_t PWM_DATATYPE;
typedef uint32_t PWM_DATATYPE2;

// PROGMEM stub (data lives in regular RAM on PC)
#define PROGMEM
#define pgm_read_byte(addr) (*(const uint8_t *)(addr))

// =============================================================================
// Configuration from hwdef.h
// =============================================================================

#define DSM_TOP       (255<<7)  // 15-bit resolution: 32640
#define RAMP_SIZE     150
#define NUM_CHANNEL_MODES 9

// Scale factors for main2 channel (two green LEDs)
#define MAIN2_SCALE     105   // 41% for CM_ALL - colorimetric D65 white
#define MAIN2_SCALE_RGG  32   // 12.5% for R+G,G (yellow/orange)
#define MAIN2_SCALE_GGB 128   // 50% for G,G+B (cyan)

// Channel mode enum from hwdef.h
enum channel_modes_e {
    CM_MAIN2 = 0,   // Green only
    CM_LED3,        // Red only
    CM_LED4,        // Blue only
    CM_ALL,         // All (balanced white)
    CM_BLEND34A,    // R+B blend
    CM_BLEND34B,    // R+G blend (yellow)
    CM_HSV,         // Hue rotation
    CM_AUTO3,       // Cyan (G+B)
    CM_CCT,         // Color temperature ramp
};

static const char *channel_names[] = {
    "CM_MAIN2 (Green)", "CM_LED3 (Red)", "CM_LED4 (Blue)",
    "CM_ALL (White)", "CM_BLEND34A (R+B)", "CM_BLEND34B (R+G)",
    "CM_HSV (Hue)", "CM_AUTO3 (Cyan)", "CM_CCT (CCT)"
};

// =============================================================================
// Ramp table (from anduril.h)
// =============================================================================

static const uint16_t pwm1_levels[RAMP_SIZE] PROGMEM = {
    0,1,2,3,4,5,6,7,9,10,12,14,17,19,22,25,28,32,36,41,45,50,56,62,69,76,84,92,
    101,110,121,132,143,156,169,184,199,215,232,251,270,291,313,336,360,386,414,
    442,473,505,539,574,612,651,693,736,782,829,880,932,987,1045,1105,1168,1233,
    1302,1374,1449,1527,1608,1693,1781,1873,1969,2068,2172,2279,2391,2507,2628,
    2753,2883,3018,3158,3303,3454,3609,3771,3938,4111,4289,4475,4666,4864,5068,
    5280,5498,5724,5957,6197,6445,6701,6965,7237,7518,7808,8106,8413,8730,9056,
    9392,9737,10093,10459,10835,11223,11621,12031,12452,12884,13329,13786,14255,
    14737,15232,15741,16262,16798,17347,17911,18489,19082,19691,20314,20954,21609,
    22281,22969,23674,24397,25137,25895,26671,27465,28279,29111,29963,30835,31727,
    32640
};

#define PWM_GET(table, level) (pgm_read_word(&(table[(level)])))
#define pgm_read_word(addr) (*(const uint16_t *)(addr))

// =============================================================================
// CCT Lookup Table (from hwdef.c)
// =============================================================================

#define CCT_LUT_SIZE 16
#define CCT_MIN_K 2700
#define CCT_MAX_K 6500

static const uint8_t cct_lut[CCT_LUT_SIZE][3] PROGMEM = {
    {255,  37,  25},  // [ 0] 2700K - warm incandescent
    {255,  40,  33},  // [ 1] 2953K
    {255,  43,  42},  // [ 2] 3206K
    {255,  45,  51},  // [ 3] 3460K
    {255,  47,  60},  // [ 4] 3713K
    {255,  49,  69},  // [ 5] 3966K
    {255,  51,  79},  // [ 6] 4220K
    {255,  53,  88},  // [ 7] 4473K
    {255,  55,  97},  // [ 8] 4726K
    {255,  56, 106},  // [ 9] 4980K
    {255,  57, 114},  // [10] 5233K
    {255,  58, 123},  // [11] 5486K
    {255,  59, 131},  // [12] 5740K
    {255,  60, 139},  // [13] 5993K
    {255,  61, 146},  // [14] 6246K
    {255,  62, 153},  // [15] 6500K - D65 daylight
};

// =============================================================================
// HSV to RGB conversion (from fsm/misc.c)
// =============================================================================

typedef struct {
    PWM_DATATYPE r;
    PWM_DATATYPE g;
    PWM_DATATYPE b;
} RGB_t;

static RGB_t hsv2rgb(uint8_t h, uint8_t s, PWM_DATATYPE v) {
    RGB_t color;

    // HSV to RGB conversion
    // h: 0-255 maps to 0-360 degrees
    // s: saturation 0-255
    // v: value (brightness) in PWM units

    uint8_t region = h / 43;
    uint8_t remainder = (h - (region * 43)) * 6;

    PWM_DATATYPE p = ((PWM_DATATYPE2)v * (255 - s)) >> 8;
    PWM_DATATYPE q = ((PWM_DATATYPE2)v * (255 - ((s * remainder) >> 8))) >> 8;
    PWM_DATATYPE t = ((PWM_DATATYPE2)v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

    switch (region) {
        case 0:  color.r = v; color.g = t; color.b = p; break;
        case 1:  color.r = q; color.g = v; color.b = p; break;
        case 2:  color.r = p; color.g = v; color.b = t; break;
        case 3:  color.r = p; color.g = q; color.b = v; break;
        case 4:  color.r = t; color.g = p; color.b = v; break;
        default: color.r = v; color.g = p; color.b = q; break;
    }

    return color;
}

// =============================================================================
// CCT calculation (from hwdef.c) - REAL CODE
// =============================================================================

static void cct_get_rgb(uint8_t cct_arg, uint8_t *r, uint8_t *g, uint8_t *b) {
    // Map 0-255 arg to LUT index (0 to CCT_LUT_SIZE-1)
    uint16_t scaled = (uint16_t)cct_arg * (CCT_LUT_SIZE - 1);
    uint8_t idx = scaled >> 8;
    uint8_t frac = scaled & 0xFF;

    if (idx >= CCT_LUT_SIZE - 1) {
        idx = CCT_LUT_SIZE - 2;
        frac = 255;
    }

    // Read adjacent LUT entries
    uint8_t r0 = pgm_read_byte(&cct_lut[idx][0]);
    uint8_t g0 = pgm_read_byte(&cct_lut[idx][1]);
    uint8_t b0 = pgm_read_byte(&cct_lut[idx][2]);
    uint8_t r1 = pgm_read_byte(&cct_lut[idx + 1][0]);
    uint8_t g1 = pgm_read_byte(&cct_lut[idx + 1][1]);
    uint8_t b1 = pgm_read_byte(&cct_lut[idx + 1][2]);

    // Linear interpolation
    *r = r0 + (((int16_t)(r1 - r0) * frac) >> 8);
    *g = g0 + (((int16_t)(g1 - g0) * frac) >> 8);
    *b = b0 + (((int16_t)(b1 - b0) * frac) >> 8);
}

// =============================================================================
// Simulated flashlight state
// =============================================================================

typedef enum {
    STATE_OFF,
    STATE_RAMP,
} sim_state_t;

static struct {
    sim_state_t state;
    uint8_t channel_mode;
    uint8_t brightness;      // 0-150 ramp level
    uint8_t channel_mode_args[NUM_CHANNEL_MODES];  // tint/hue args
    int ramp_direction;
} light = {
    .state = STATE_OFF,
    .channel_mode = CM_ALL,
    .brightness = 75,
    .channel_mode_args = {0, 0, 0, 0, 128, 128, 213, 0, 128},  // from CHANNEL_MODE_ARGS
    .ramp_direction = 1
};

// =============================================================================
// Calculate LED output using REAL firmware logic
// =============================================================================

static void calculate_led_output(uint16_t *main2, uint16_t *led3, uint16_t *led4) {
    if (light.state == STATE_OFF) {
        *main2 = *led3 = *led4 = 0;
        return;
    }

    PWM_DATATYPE brightness = PWM_GET(pwm1_levels, light.brightness);

    switch (light.channel_mode) {
        case CM_MAIN2:  // Green only (from set_level_main2)
            *main2 = brightness;
            *led3 = *led4 = 0;
            break;

        case CM_LED3:   // Red only (from set_level_led3)
            *led3 = brightness;
            *main2 = *led4 = 0;
            break;

        case CM_LED4:   // Blue only (from set_level_led4)
            *led4 = brightness;
            *main2 = *led3 = 0;
            break;

        case CM_ALL:    // Balanced white (from set_level_all)
            // Scale down main2 (green LEDs) for balanced color mixing
            // THIS IS THE REAL FIRMWARE CODE
            *main2 = ((PWM_DATATYPE2)brightness * MAIN2_SCALE) >> 8;
            *led3 = brightness;
            *led4 = brightness;
            break;

        case CM_BLEND34A:  // R+B blend
            {
                uint8_t blend = light.channel_mode_args[CM_BLEND34A];
                // Simplified 2-channel blend
                *led3 = ((PWM_DATATYPE2)brightness * (255 - blend)) >> 8;
                *led4 = ((PWM_DATATYPE2)brightness * blend) >> 8;
                *main2 = 0;
            }
            break;

        case CM_BLEND34B:  // R+G,G (yellow/orange) (from set_level_led34b_blend)
            // Scale main2 (green LEDs) lower for better yellow/orange
            *main2 = ((PWM_DATATYPE2)brightness * MAIN2_SCALE_RGG) >> 8;
            *led3 = brightness;
            *led4 = 0;
            break;

        case CM_HSV:   // HSV mode (from set_level_hsv)
            {
                uint8_t h = light.channel_mode_args[CM_HSV];
                RGB_t color = hsv2rgb(h, 255, brightness);
                // Scale down main2 (green LEDs) for balanced color mixing
                // THIS IS THE REAL FIRMWARE CODE
                *main2 = ((PWM_DATATYPE2)color.g * MAIN2_SCALE) >> 8;
                *led3 = color.r;
                *led4 = color.b;
            }
            break;

        case CM_AUTO3:  // Cyan G,G+B (from set_level_auto3)
            // Scale main2 (green LEDs) higher for better cyan
            *main2 = ((PWM_DATATYPE2)brightness * MAIN2_SCALE_GGB) >> 8;
            *led3 = 0;
            *led4 = brightness;
            break;

        case CM_CCT:   // CCT mode (from set_level_cct)
            {
                uint8_t cct_arg = light.channel_mode_args[CM_CCT];
                uint8_t r_ratio, g_ratio, b_ratio;

                // THIS IS THE REAL FIRMWARE CODE
                cct_get_rgb(cct_arg, &r_ratio, &g_ratio, &b_ratio);

                // Scale by brightness
                *led3 = ((PWM_DATATYPE2)brightness * r_ratio) >> 8;
                *main2 = ((PWM_DATATYPE2)brightness * g_ratio) >> 8;
                *led4 = ((PWM_DATATYPE2)brightness * b_ratio) >> 8;
            }
            break;

        default:
            *main2 = *led3 = *led4 = 0;
            break;
    }
}

// =============================================================================
// Input handling
// =============================================================================

static volatile int running = 1;

static void handle_signal(int sig) {
    (void)sig;
    running = 0;
}

static char get_key(void) {
    char c = 0;
    if (read(STDIN_FILENO, &c, 1) == 1) return c;
    return 0;
}

// =============================================================================
// Main loop
// =============================================================================

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    render_init();
    atexit(render_cleanup);

    printf("D4K-3ch Test Harness (REAL channel mode code)\n");
    printf("Testing: CCT, HSV, and balanced white calculations\n");
    usleep(1000000);

    while (running) {
        char key = get_key();

        switch (key) {
            case 'q':
            case 'Q':
                running = 0;
                break;

            case 'c':  // Click - toggle on/off
                light.state = (light.state == STATE_OFF) ? STATE_RAMP : STATE_OFF;
                break;

            case '+':
            case '=':
                if (light.brightness < RAMP_SIZE - 1)
                    light.brightness++;
                if (light.state == STATE_OFF) light.state = STATE_RAMP;
                break;

            case '-':
            case '_':
                if (light.brightness > 1)
                    light.brightness--;
                break;

            case 'm':
            case 'M':  // Cycle channel modes
                light.channel_mode = (light.channel_mode + 1) % NUM_CHANNEL_MODES;
                break;

            case '[':  // Decrease tint/hue/CCT
                if (light.channel_mode_args[light.channel_mode] >= 4)
                    light.channel_mode_args[light.channel_mode] -= 4;
                else
                    light.channel_mode_args[light.channel_mode] = 0;
                break;

            case ']':  // Increase tint/hue/CCT
                if (light.channel_mode_args[light.channel_mode] <= 251)
                    light.channel_mode_args[light.channel_mode] += 4;
                else
                    light.channel_mode_args[light.channel_mode] = 255;
                break;

            case 'h':  // Hold - ramp brightness
                light.brightness += light.ramp_direction * 3;
                if (light.brightness >= RAMP_SIZE - 1) {
                    light.brightness = RAMP_SIZE - 1;
                    light.ramp_direction = -1;
                } else if (light.brightness <= 1) {
                    light.brightness = 1;
                    light.ramp_direction = 1;
                }
                if (light.state == STATE_OFF) light.state = STATE_RAMP;
                break;

            default:
                break;
        }

        // Calculate LED output using REAL firmware logic
        uint16_t main2, led3, led4;
        calculate_led_output(&main2, &led3, &led4);

        // Convert to 8-bit RGB for display
        uint8_t r = led3 >> 7;   // Red
        uint8_t g = main2 >> 7;  // Green
        uint8_t b = led4 >> 7;   // Blue

        // Get tint value for display
        uint8_t tint_display = light.channel_mode_args[light.channel_mode];

        // Calculate CCT in Kelvin for display
        char cct_info[32] = "";
        if (light.channel_mode == CM_CCT) {
            uint16_t cct_k = CCT_MIN_K +
                ((uint32_t)(CCT_MAX_K - CCT_MIN_K) * tint_display) / 255;
            snprintf(cct_info, sizeof(cct_info), " (~%dK)", cct_k);
        }

        // Render display
        const char *state_name = (light.state == STATE_OFF) ? "off_mode" : "ramp_mode";
        render_display(r, g, b,
                       main2, led3, led4,
                       state_name,
                       channel_names[light.channel_mode],
                       light.brightness, RAMP_SIZE - 1,
                       tint_display);

        // Add CCT info if in CCT mode
        if (light.channel_mode == CM_CCT && cct_info[0]) {
            printf("  CCT: %s\n", cct_info);
        }

        // ~60 FPS
        usleep(16000);
    }

    return 0;
}
