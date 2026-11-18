#include <stdio.h>
#include <string.h>
#include "hal.h"
#include "sched.h"
#include "app.h"

#define DEFAULT_SECONDS  (30UL * 60UL)   /* 30 minutes if user sets nothing */
#define DEBOUNCE_MS      50u
#define BUZZ_TOGGLES     20u             /* 20 x 500 ms = 10 s of beeping */

enum { T_BUTTON, T_LCD, T_COUNTER, T_BUZZER, T_COUNT };
typedef enum { ST_IDLE, ST_SETTING, ST_RUNNING, ST_DONE } state_t;

static state_t  g_state = ST_IDLE;
static uint8_t  g_hh, g_mm, g_ss;          /* values the user is setting */
static uint32_t g_remaining;               /* single source of truth while running */
static uint8_t  g_buzz_toggles;
static bool     g_buzz_on;

static volatile uint8_t  g_pending;        /* one bit per button, set in ISR */
static volatile uint32_t g_last_press[BTN_COUNT];

/* ---------- ISR side: tiny and fast ---------- */
void app_button_isr(button_t id)
{
    uint32_t now = hal_millis();
    if ((uint32_t)(now - g_last_press[id]) >= DEBOUNCE_MS) {
        g_last_press[id] = now;
        g_pending |= (uint8_t)(1u << id);
    }
}

/* ---------- helpers ---------- */
static void silence(void)
{
    sched_enable(T_BUZZER, false);
    g_buzz_on = false;
    hal_buzzer_set(false);
}

static void adjust(button_t b)
{
    switch (b) {
    case BTN_H_UP: g_hh = (g_hh < 23) ? g_hh + 1 : 0;  break;
    case BTN_H_DN: g_hh = (g_hh > 0)  ? g_hh - 1 : 23; break;
    case BTN_M_UP: g_mm = (g_mm < 59) ? g_mm + 1 : 0;  break;
    case BTN_M_DN: g_mm = (g_mm > 0)  ? g_mm - 1 : 59; break;
    case BTN_S_UP: g_ss = (g_ss < 59) ? g_ss + 1 : 0;  break;
    case BTN_S_DN: g_ss = (g_ss > 0)  ? g_ss - 1 : 59; break;
    default: break;
    }
}

static void start_countdown(void)
{
    g_remaining = (uint32_t)g_hh * 3600UL + (uint32_t)g_mm * 60UL + g_ss;
    if (g_remaining == 0) g_remaining = DEFAULT_SECONDS;
    g_state = ST_RUNNING;
    sched_set_period(T_LCD, 1000);
    sched_enable(T_COUNTER, true);
}

static void handle_button(button_t b)
{
    if (g_state == ST_DONE) {              /* any button silences the alarm */
        silence();
        g_state = ST_IDLE;
        return;
    }
    if (b == BTN_START) {                  /* START works from any state */
        start_countdown();
        return;
    }
    if (g_state == ST_RUNNING) return;     /* ignore edits while counting */
    g_state = ST_SETTING;
    sched_set_period(T_LCD, 200);          /* faster refresh while editing */
    adjust(b);
}

/* ---------- tasks ---------- */
static void button_task(void)
{
    hal_irq_disable();
    uint8_t p = g_pending;
    g_pending = 0;
    hal_irq_enable();

    for (uint8_t i = 0; i < BTN_COUNT; i++)
        if (p & (1u << i)) handle_button((button_t)i);
}

static void counter_task(void)
{
    if (g_remaining > 0) g_remaining--;
    if (g_remaining == 0) {
        g_state = ST_DONE;
        sched_enable(T_COUNTER, false);
        g_buzz_toggles = 0;
        sched_enable(T_BUZZER, true);
    }
}

static void buzzer_task(void)
{
    g_buzz_on = !g_buzz_on;
    hal_buzzer_set(g_buzz_on);
    if (++g_buzz_toggles >= BUZZ_TOGGLES) {
        silence();
        g_state = ST_IDLE;
    }
}

static void lcd_task(void)
{
    char l0[17], l1[17];
    unsigned h, m, s;

    switch (g_state) {
    case ST_IDLE:
        snprintf(l0, sizeof l0, "Pomodoro Timer");
        snprintf(l1, sizeof l1, "Set time & START");
        break;
    case ST_SETTING:
        snprintf(l0, sizeof l0, "SET  %02u:%02u:%02u", (unsigned)g_hh, (unsigned)g_mm, (unsigned)g_ss);
        snprintf(l1, sizeof l1, "START = go");
        break;
    case ST_RUNNING:
        h = (unsigned)(g_remaining / 3600UL);
        m = (unsigned)((g_remaining % 3600UL) / 60UL);
        s = (unsigned)(g_remaining % 60UL);
        snprintf(l0, sizeof l0, "     %02u:%02u:%02u", h, m, s);
        snprintf(l1, sizeof l1, "Stay focused!");
        break;
    default: /* ST_DONE */
        snprintf(l0, sizeof l0, "Time is up!");
        snprintf(l1, sizeof l1, "Press any button");
        break;
    }
    hal_lcd_print(0, l0);
    hal_lcd_print(1, l1);
}

/* ---------- task table (positional init, valid in C and C++) ---------- */
static task_t g_tasks[T_COUNT] = {
    { button_task,  20,   0, true  },   /* T_BUTTON  */
    { lcd_task,     1000, 0, true  },   /* T_LCD     */
    { counter_task, 1000, 0, false },   /* T_COUNTER */
    { buzzer_task,  500,  0, false }    /* T_BUZZER  */
};

void app_init(void) { sched_init(g_tasks, T_COUNT); }
void app_run(void)  { sched_run(); hal_watchdog_kick(); }
