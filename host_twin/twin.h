/* Digital-twin test/observation interface (host only) */
#ifndef TWIN_H
#define TWIN_H
#include <stdint.h>
#include <stdbool.h>
void          twin_reset(void);
void          twin_clock_set(uint32_t ms);
const char   *twin_lcd_row(int r);
bool          twin_buzzer(void);
unsigned long twin_lcd_writes(void);
unsigned long twin_buzzer_edges(void);
uint32_t      twin_wdt_max_gap(void);
#endif
