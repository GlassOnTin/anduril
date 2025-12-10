// sim-render.h: Terminal rendering for D4K-3ch emulator
#pragma once

#include <stdint.h>

// Initialize terminal for raw input and true color output
void render_init(void);

// Restore terminal settings on exit
void render_cleanup(void);

// Clear screen and move cursor to home
void render_clear(void);

// Render the full display (beam, channel bars, status)
void render_display(uint8_t r, uint8_t g, uint8_t b,
                    uint16_t main2_pwm, uint16_t led3_pwm, uint16_t led4_pwm,
                    const char *state_name, const char *channel_name,
                    uint8_t brightness, uint8_t max_brightness,
                    uint8_t tint_value);

// Individual render components
void render_beam(uint8_t r, uint8_t g, uint8_t b);
void render_channel_bars(uint16_t main2, uint16_t led3, uint16_t led4);
void render_status(const char *state, const char *channel,
                   uint8_t brightness, uint8_t max_brightness,
                   uint8_t tint);
void render_help(void);
