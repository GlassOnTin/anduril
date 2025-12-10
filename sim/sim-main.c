// sim-main.c: Main emulator for D4K-3ch flashlight
// Standalone simulator demonstrating the terminal visualization
// Future work: integrate actual Anduril firmware code

#define _DEFAULT_SOURCE  // For usleep()

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>

#include "sim-hwdef.h"
#include "sim-render.h"

// =============================================================================
// Simulated Flashlight State
// =============================================================================

typedef enum {
    STATE_OFF,
    STATE_RAMP,
    STATE_STROBE,
    STATE_CONFIG
} sim_state_t;

typedef enum {
    CM_MAIN2 = 0,   // Green only
    CM_LED3,        // Red only
    CM_LED4,        // Blue only
    CM_ALL,         // All (balanced white)
    CM_BLEND34A,    // R+B blend
    CM_BLEND34B,    // R+G blend (yellow)
    CM_HSV,         // Hue rotation
    CM_AUTO3,       // Cyan (G+B)
    CM_CCT,         // Color temperature ramp
    NUM_CHANNEL_MODES
} channel_mode_t;

static const char *state_names[] = {
    "off_mode", "ramp_mode", "strobe_mode", "config_mode"
};

static const char *channel_names[] = {
    "CM_MAIN2 (Green)", "CM_LED3 (Red)", "CM_LED4 (Blue)",
    "CM_ALL (White)", "CM_BLEND34A (R+B)", "CM_BLEND34B (R+G)",
    "CM_HSV (Hue)", "CM_AUTO3 (Cyan)", "CM_CCT (CCT)"
};

// CCT lookup table (from hwdef.c)
static const uint8_t cct_lut[16][3] = {
    {255,  37,  25},  // 2700K
    {255,  40,  33},
    {255,  43,  42},
    {255,  45,  51},
    {255,  47,  60},
    {255,  49,  69},
    {255,  51,  79},
    {255,  53,  88},
    {255,  55,  97},
    {255,  56, 106},
    {255,  57, 114},
    {255,  58, 123},
    {255,  59, 131},
    {255,  60, 139},
    {255,  61, 146},
    {255,  62, 153},  // 6500K
};

// Flashlight state
static struct {
    sim_state_t state;
    channel_mode_t channel;
    uint8_t brightness;      // 0-150 ramp level
    uint8_t tint;            // 0-255 for HSV/CCT modes
    uint8_t hue;             // 0-255 for HSV mode
    int ramp_direction;      // +1 or -1
} light = {
    .state = STATE_OFF,
    .channel = CM_ALL,
    .brightness = 75,
    .tint = 128,
    .hue = 0,
    .ramp_direction = 1
};

#define RAMP_MAX 150
#define DSM_TOP 32767

// =============================================================================
// LED Output Calculation
// =============================================================================

// Scale factors for RGBG balance (from hwdef.c)
#define MAIN2_SCALE     105  // 41% for white balance
#define MAIN2_SCALE_RGG  32  // 12.5% for yellow
#define MAIN2_SCALE_GGB 128  // 50% for cyan

static void calculate_led_output(uint16_t *main2, uint16_t *led3, uint16_t *led4) {
    if (light.state == STATE_OFF) {
        *main2 = *led3 = *led4 = 0;
        return;
    }

    // Convert brightness level to PWM (simplified exponential ramp)
    uint32_t pwm = ((uint32_t)light.brightness * light.brightness * DSM_TOP) / (RAMP_MAX * RAMP_MAX);
    if (pwm > DSM_TOP) pwm = DSM_TOP;

    switch (light.channel) {
        case CM_MAIN2:  // Green only
            *main2 = pwm;
            *led3 = *led4 = 0;
            break;

        case CM_LED3:   // Red only
            *led3 = pwm;
            *main2 = *led4 = 0;
            break;

        case CM_LED4:   // Blue only
            *led4 = pwm;
            *main2 = *led3 = 0;
            break;

        case CM_ALL:    // Balanced white
            *led3 = pwm;
            *led4 = pwm;
            *main2 = (pwm * MAIN2_SCALE) >> 8;
            break;

        case CM_BLEND34A:  // Red + Blue (purple/magenta)
            *led3 = pwm;
            *led4 = pwm;
            *main2 = 0;
            break;

        case CM_BLEND34B:  // Red + Green (yellow/orange)
            *led3 = pwm;
            *main2 = (pwm * MAIN2_SCALE_RGG) >> 8;
            *led4 = 0;
            break;

        case CM_HSV: {  // Hue rotation
            // Simple HSV to RGB (saturation=255, value=pwm)
            uint8_t h = light.hue;
            uint8_t region = h / 43;
            uint8_t remainder = (h - (region * 43)) * 6;
            uint8_t p = 0;
            uint8_t q = (255 - remainder);
            uint8_t t = remainder;

            uint8_t r, g, b;
            switch (region) {
                case 0:  r = 255; g = t;   b = p;   break;
                case 1:  r = q;   g = 255; b = p;   break;
                case 2:  r = p;   g = 255; b = t;   break;
                case 3:  r = p;   g = q;   b = 255; break;
                case 4:  r = t;   g = p;   b = 255; break;
                default: r = 255; g = p;   b = q;   break;
            }

            *led3 = (pwm * r) >> 8;
            *main2 = ((pwm * g) >> 8) * MAIN2_SCALE >> 8;
            *led4 = (pwm * b) >> 8;
            break;
        }

        case CM_AUTO3:  // Cyan (Green + Blue)
            *main2 = (pwm * MAIN2_SCALE_GGB) >> 8;
            *led3 = 0;
            *led4 = pwm;
            break;

        case CM_CCT: {  // Color temperature ramp
            // Interpolate CCT lookup table
            uint16_t idx_scaled = (uint16_t)light.tint * 15;
            uint8_t idx = idx_scaled >> 8;
            uint8_t frac = idx_scaled & 0xFF;

            if (idx >= 15) { idx = 14; frac = 255; }

            uint8_t r = cct_lut[idx][0] + (((int16_t)(cct_lut[idx+1][0] - cct_lut[idx][0]) * frac) >> 8);
            uint8_t g = cct_lut[idx][1] + (((int16_t)(cct_lut[idx+1][1] - cct_lut[idx][1]) * frac) >> 8);
            uint8_t b = cct_lut[idx][2] + (((int16_t)(cct_lut[idx+1][2] - cct_lut[idx][2]) * frac) >> 8);

            *led3 = (pwm * r) >> 8;
            *main2 = (pwm * g) >> 8;
            *led4 = (pwm * b) >> 8;
            break;
        }

        default:
            *main2 = *led3 = *led4 = 0;
            break;
    }
}

// =============================================================================
// Input Handling
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

static void handle_click(void) {
    if (light.state == STATE_OFF) {
        light.state = STATE_RAMP;
    } else {
        light.state = STATE_OFF;
    }
}

static void handle_hold(void) {
    if (light.state == STATE_OFF) {
        // Turn on and start ramping
        light.state = STATE_RAMP;
    }
    // Ramp brightness
    light.brightness += light.ramp_direction * 3;
    if (light.brightness >= RAMP_MAX) {
        light.brightness = RAMP_MAX;
        light.ramp_direction = -1;
    } else if (light.brightness <= 1) {
        light.brightness = 1;
        light.ramp_direction = 1;
    }
}

static void handle_3h(void) {
    // 3H cycles channel modes or adjusts tint
    if (light.channel == CM_HSV) {
        light.hue = (light.hue + 8) & 0xFF;
    } else if (light.channel == CM_CCT) {
        light.tint = (light.tint + 8) & 0xFF;
    } else {
        light.channel = (light.channel + 1) % NUM_CHANNEL_MODES;
    }
}

// =============================================================================
// Main Loop
// =============================================================================

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    // Set up signal handler for clean exit
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Initialize
    sim_hw_init();
    render_init();

    // Register cleanup
    atexit(render_cleanup);

    printf("D4K-3ch Emulator starting...\n");
    usleep(500000);

    int hold_counter = 0;

    while (running) {
        char key = get_key();

        switch (key) {
            case 'q':
            case 'Q':
                running = 0;
                break;

            case 'c':  // Single click
                handle_click();
                break;

            case '2':  // Double click - not implemented yet
                break;

            case '3':  // Triple click - channel mode
                handle_3h();
                break;

            case 'p':  // Press (start hold)
                hold_counter = 1;
                break;

            case 'r':  // Release
                if (hold_counter > 0 && hold_counter < 10) {
                    // Short press = click
                    handle_click();
                }
                hold_counter = 0;
                break;

            case 'h':  // Hold tick
                handle_hold();
                break;

            case '+':
            case '=':
                if (light.brightness < RAMP_MAX)
                    light.brightness++;
                break;

            case '-':
            case '_':
                if (light.brightness > 1)
                    light.brightness--;
                break;

            case '[':
                if (light.tint > 0) light.tint -= 4;
                break;

            case ']':
                if (light.tint < 255) light.tint += 4;
                break;

            case '<':
            case ',':
                light.hue = (light.hue - 8) & 0xFF;
                break;

            case '>':
            case '.':
                light.hue = (light.hue + 8) & 0xFF;
                break;

            case 'm':
            case 'M':
                light.channel = (light.channel + 1) % NUM_CHANNEL_MODES;
                break;

            default:
                break;
        }

        // Process hold
        if (hold_counter > 0) {
            hold_counter++;
            if (hold_counter > 10) {
                handle_hold();
            }
        }

        // Calculate LED output
        uint16_t main2, led3, led4;
        calculate_led_output(&main2, &led3, &led4);

        // Store in simulated hardware state
        sim_hw.main2_pwm = main2;
        sim_hw.led3_pwm = led3;
        sim_hw.led4_pwm = led4;

        // Convert to 8-bit RGB for display
        uint8_t r = led3 >> 7;
        uint8_t g = main2 >> 7;
        uint8_t b = led4 >> 7;

        // Get tint value for display
        uint8_t tint_display = 0;
        if (light.channel == CM_CCT) {
            tint_display = light.tint;
        } else if (light.channel == CM_HSV) {
            tint_display = light.hue;
        }

        // Render display
        render_display(r, g, b,
                       main2, led3, led4,
                       state_names[light.state],
                       channel_names[light.channel],
                       light.brightness, RAMP_MAX,
                       tint_display);

        // ~60 FPS
        usleep(16000);
    }

    return 0;
}
