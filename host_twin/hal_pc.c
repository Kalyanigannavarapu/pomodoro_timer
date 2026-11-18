/* Virtual hardware: replaces port_arduino.ino on a PC.
 * Clock is owned by the driver (twin_main.c), so time is deterministic. */
#include <string.h>
#include "hal.h"
#include "twin.h"

static uint32_t g_clock;
static char     g_lcd[2][17];
static bool     g_buzzer;
static unsigned long g_lcd_writes, g_edges;
static uint32_t g_last_kick;
static uint32_t g_max_gap;

void twin_reset(void)
{
    g_clock = 0; g_buzzer = false; g_lcd_writes = g_edges = 0;
    g_last_kick = 0; g_max_gap = 0;
    for (int r = 0; r < 2; r++) { memset(g_lcd[r], ' ', 16); g_lcd[r][16] = 0; }
}
void twin_clock_set(uint32_t ms)  { g_clock = ms; }
const char *twin_lcd_row(int r)   { return g_lcd[r & 1]; }
bool twin_buzzer(void)            { return g_buzzer; }
unsigned long twin_lcd_writes(void)   { return g_lcd_writes; }
unsigned long twin_buzzer_edges(void) { return g_edges; }
uint32_t twin_wdt_max_gap(void)   { return g_max_gap; }

void     hal_init(void)           { g_last_kick = g_clock; }
uint32_t hal_millis(void)         { return g_clock; }
void     hal_irq_disable(void)    { /* single thread: nothing to mask */ }
void     hal_irq_enable(void)     { }

void hal_lcd_print(uint8_t row, const char *text)
{
    size_t n = strlen(text);
    if (n > 16) n = 16;
    memset(g_lcd[row & 1], ' ', 16);
    memcpy(g_lcd[row & 1], text, n);
    g_lcd_writes++;
}

void hal_buzzer_set(bool on)
{
    if (on != g_buzzer) g_edges++;
    g_buzzer = on;
}

void hal_watchdog_kick(void)
{
    uint32_t gap = g_clock - g_last_kick;
    if (gap > g_max_gap) g_max_gap = gap;
    g_last_kick = g_clock;
}
