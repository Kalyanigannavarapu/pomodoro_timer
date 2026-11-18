#include <Arduino.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <string.h>
#include <LiquidCrystal_I2C.h>
#include "hal.h"
#include "app.h"

#define PIN_START   2
#define PIN_BUZZER  3
#define BTN_MASK    0x3F          /* D8..D13 = PB0..PB5 = BTN_H_UP..BTN_S_DN */

static LiquidCrystal_I2C lcd(0x27, 16, 2);   /* try 0x3F if blank */
static volatile uint8_t prev_pinb = BTN_MASK;

static void start_isr(void) { app_button_isr(BTN_START); }

ISR(PCINT0_vect)
{
    uint8_t now  = PINB & BTN_MASK;
    uint8_t fell = prev_pinb & (uint8_t)~now;     /* HIGH to LOW = pressed */
    prev_pinb = now;
    for (uint8_t i = 0; i < 6; i++)
        if (fell & (1u << i)) app_button_isr((button_t)i);
}

void hal_init(void)
{
    DDRB  &= (uint8_t)~BTN_MASK;      /* inputs */
    PORTB |= BTN_MASK;                /* pull-ups on */
    pinMode(PIN_START, INPUT_PULLUP);
    pinMode(PIN_BUZZER, OUTPUT);
    lcd.init();
    lcd.backlight();
    PCICR  |= (1 << PCIE0);
    PCMSK0 |= BTN_MASK;
    attachInterrupt(digitalPinToInterrupt(PIN_START), start_isr, FALLING);
    wdt_enable(WDTO_2S);
}

uint32_t hal_millis(void)        { return millis(); }
void hal_irq_disable(void)       { noInterrupts(); }
void hal_irq_enable(void)        { interrupts(); }
void hal_buzzer_set(bool on)     { digitalWrite(PIN_BUZZER, on ? HIGH : LOW); }
void hal_watchdog_kick(void)     { wdt_reset(); }

void hal_lcd_print(uint8_t row, const char *text)
{
    lcd.setCursor(0, row);
    lcd.print(text);
    for (size_t i = strlen(text); i < 16; i++) lcd.print(' ');
}

void setup() { hal_init(); app_init(); }
void loop()  { app_run(); }
