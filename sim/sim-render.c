// sim-render.c: Terminal rendering for D4K-3ch emulator
// Uses ANSI escape codes for true color (24-bit) output

#include "sim-render.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

// =============================================================================
// ANSI Escape Code Helpers
// =============================================================================

#define ANSI_CLEAR       "\x1b[2J"
#define ANSI_HOME        "\x1b[H"
#define ANSI_RESET       "\x1b[0m"
#define ANSI_BOLD        "\x1b[1m"
#define ANSI_DIM         "\x1b[2m"
#define ANSI_HIDE_CURSOR "\x1b[?25l"
#define ANSI_SHOW_CURSOR "\x1b[?25h"

// True color (24-bit) foreground
#define FG_RGB(r,g,b) printf("\x1b[38;2;%d;%d;%dm", (r), (g), (b))
// True color (24-bit) background
#define BG_RGB(r,g,b) printf("\x1b[48;2;%d;%d;%dm", (r), (g), (b))
// Reset colors
#define RESET() printf(ANSI_RESET)

// Move cursor to row, col (1-indexed)
#define MOVE_TO(row, col) printf("\x1b[%d;%dH", (row), (col))

// =============================================================================
// Terminal Setup
// =============================================================================

static struct termios orig_termios;
static int terminal_initialized = 0;

void render_init(void) {
    if (terminal_initialized) return;

    // Save original terminal settings
    tcgetattr(STDIN_FILENO, &orig_termios);

    // Set up raw mode (no echo, no line buffering)
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;   // Non-blocking read
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    // Hide cursor and clear screen
    printf(ANSI_HIDE_CURSOR);
    printf(ANSI_CLEAR ANSI_HOME);
    fflush(stdout);

    terminal_initialized = 1;
}

void render_cleanup(void) {
    if (!terminal_initialized) return;

    // Restore terminal settings
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);

    // Show cursor and reset colors
    printf(ANSI_SHOW_CURSOR ANSI_RESET "\n");
    fflush(stdout);

    terminal_initialized = 0;
}

void render_clear(void) {
    printf(ANSI_CLEAR ANSI_HOME);
}

// =============================================================================
// Beam Visualization
// =============================================================================

// Circular beam pattern using Unicode block characters
static const char *beam_pattern[] = {
    "      ██████████      ",
    "    ██████████████    ",
    "  ████████████████████  ",
    " ██████████████████████ ",
    "████████████████████████",
    "████████████████████████",
    "████████████████████████",
    "████████████████████████",
    " ██████████████████████ ",
    "  ████████████████████  ",
    "    ██████████████    ",
    "      ██████████      ",
};
#define BEAM_ROWS 12

void render_beam(uint8_t r, uint8_t g, uint8_t b) {
    // If LED is off, show dim gray
    if (r == 0 && g == 0 && b == 0) {
        FG_RGB(30, 30, 30);
    } else {
        FG_RGB(r, g, b);
    }

    for (int i = 0; i < BEAM_ROWS; i++) {
        printf("  %s\n", beam_pattern[i]);
    }
    RESET();
}

// =============================================================================
// Channel Level Bars
// =============================================================================

#define BAR_WIDTH 24
#define DSM_MAX 32767  // 15-bit max

void render_channel_bars(uint16_t main2, uint16_t led3, uint16_t led4) {
    // Red channel (led3)
    {
        int filled = (led3 * BAR_WIDTH) / DSM_MAX;
        if (filled > BAR_WIDTH) filled = BAR_WIDTH;
        int pct = (led3 * 100) / DSM_MAX;
        printf("  R ");
        FG_RGB(255, 60, 60);
        for (int i = 0; i < filled; i++) printf("█");
        FG_RGB(60, 60, 60);
        for (int i = filled; i < BAR_WIDTH; i++) printf("░");
        RESET();
        printf(" %5d (%3d%%)\n", led3, pct);
    }

    // Green channel (main2) - note: 2 physical LEDs
    {
        int filled = (main2 * BAR_WIDTH) / DSM_MAX;
        if (filled > BAR_WIDTH) filled = BAR_WIDTH;
        int pct = (main2 * 100) / DSM_MAX;
        printf("  G ");
        FG_RGB(60, 255, 60);
        for (int i = 0; i < filled; i++) printf("█");
        FG_RGB(60, 60, 60);
        for (int i = filled; i < BAR_WIDTH; i++) printf("░");
        RESET();
        printf(" %5d (%3d%%) [x2 LEDs]\n", main2, pct);
    }

    // Blue channel (led4)
    {
        int filled = (led4 * BAR_WIDTH) / DSM_MAX;
        if (filled > BAR_WIDTH) filled = BAR_WIDTH;
        int pct = (led4 * 100) / DSM_MAX;
        printf("  B ");
        FG_RGB(60, 120, 255);
        for (int i = 0; i < filled; i++) printf("█");
        FG_RGB(60, 60, 60);
        for (int i = filled; i < BAR_WIDTH; i++) printf("░");
        RESET();
        printf(" %5d (%3d%%)\n", led4, pct);
    }
}

// =============================================================================
// Status Display
// =============================================================================

void render_status(const char *state, const char *channel,
                   uint8_t brightness, uint8_t max_brightness,
                   uint8_t tint) {
    printf("\n");
    printf(ANSI_BOLD "  State:   " ANSI_RESET "%s\n", state ? state : "unknown");
    printf(ANSI_BOLD "  Channel: " ANSI_RESET "%s\n", channel ? channel : "unknown");
    printf(ANSI_BOLD "  Level:   " ANSI_RESET "%d / %d\n", brightness, max_brightness);
    if (tint > 0) {
        printf(ANSI_BOLD "  Tint:    " ANSI_RESET "%d (0=warm, 255=cool)\n", tint);
    }
}

// =============================================================================
// Help Display
// =============================================================================

void render_help(void) {
    printf("\n");
    FG_RGB(180, 180, 180);
    printf("  ─────────────────────────────────────\n");
    printf("  [p] press   [r] release   [h] hold\n");
    printf("  [c] click   [2] 2-click   [3] 3-click\n");
    printf("  [q] quit    [?] help\n");
    printf("  ─────────────────────────────────────\n");
    RESET();
}

// =============================================================================
// Full Display Render
// =============================================================================

void render_display(uint8_t r, uint8_t g, uint8_t b,
                    uint16_t main2_pwm, uint16_t led3_pwm, uint16_t led4_pwm,
                    const char *state_name, const char *channel_name,
                    uint8_t brightness, uint8_t max_brightness,
                    uint8_t tint_value) {

    // Move to home position (don't clear - reduces flicker)
    printf(ANSI_HOME);

    // Title
    printf("\n");
    FG_RGB(100, 200, 255);
    printf(ANSI_BOLD "  ═══ D4K-3ch Emulator ═══\n" ANSI_RESET);
    printf("\n");

    // Beam visualization
    render_beam(r, g, b);

    printf("\n");

    // Channel level bars
    render_channel_bars(main2_pwm, led3_pwm, led4_pwm);

    // Status info
    render_status(state_name, channel_name, brightness, max_brightness, tint_value);

    // Help
    render_help();

    // Flush output
    fflush(stdout);
}
