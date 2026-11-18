#ifndef HAL_H
#define HAL_H
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    BTN_H_UP, BTN_H_DN, BTN_M_UP, BTN_M_DN,
    BTN_S_UP, BTN_S_DN, BTN_START, BTN_COUNT
} button_t;

/* Implemented by each port */
void     hal_init(void);
uint32_t hal_millis(void);
void     hal_irq_disable(void);
void     hal_irq_enable(void);
void     hal_lcd_print(uint8_t row, const char *text);  /* pads to 16 chars */
void     hal_buzzer_set(bool on);
void     hal_watchdog_kick(void);

/* Implemented by app.c, called by the port from the button ISR */
void     app_button_isr(button_t id);
#endif
