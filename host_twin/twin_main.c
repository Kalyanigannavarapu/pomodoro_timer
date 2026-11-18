/* Pomodoro digital twin driver.
 *   ./twin               interactive terminal twin (keyboard = buttons)
 *   ./twin --speed 60    interactive, simulated time runs 60x faster
 *   ./twin --test        automated scenarios (each in a fresh "power cycle")
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/wait.h>
#include "hal.h"
#include "app.h"
#include "twin.h"

/* ====================== simulation helpers ====================== */
static uint32_t now;                       /* simulated ms (wraps like millis()) */

static void tick(void) { now++; twin_clock_set(now); app_run(); }   /* one super-loop pass per ms */
static void run_ms(uint32_t ms) { while (ms--) tick(); }

static void power_on(uint32_t t0)
{
    twin_reset(); now = t0; twin_clock_set(now);
    hal_init(); app_init();
}

static uint32_t t_press;
static void tap(button_t b) { t_press = now; app_button_isr(b); run_ms(60); }  /* press + settle */

/* run until buzzer first turns on; returns ms elapsed since 'since' (or 0xFFFFFFFF) */
static uint32_t until_buzzer(uint32_t since, uint32_t limit_ms)
{
    for (uint32_t i = 0; i < limit_ms; i++) { tick(); if (twin_buzzer()) return now - since; }
    return 0xFFFFFFFFu;
}

static int lcd_seconds(void)   /* parse "SET  hh:mm:ss" or "     hh:mm:ss" */
{
    unsigned h, m, s;
    const char *r = twin_lcd_row(0);
    if (sscanf(r + 5, "%2u:%2u:%2u", &h, &m, &s) == 3) return (int)(h * 3600 + m * 60 + s);
    return -1;
}

/* ====================== tiny test framework ====================== */
static int g_fail;
#define CHECK(c, msg) do { if (c) printf("    ok    %s\n", msg); \
                           else { printf("    FAIL  %s  (line %d)\n", msg, __LINE__); g_fail++; } } while (0)
#define ROW0_IS(s) (strncmp(twin_lcd_row(0), (s), strlen(s)) == 0)
#define ROW1_IS(s) (strncmp(twin_lcd_row(1), (s), strlen(s)) == 0)

/* ====================== scenarios ====================== */
static void s_boot(void)
{
    power_on(0); run_ms(1100);
    CHECK(ROW0_IS("Pomodoro Timer"), "splash line 1");
    CHECK(ROW1_IS("Set time & START"), "splash line 2");
    CHECK(!twin_buzzer(), "buzzer silent at boot");
}

static void s_set_and_run(void)
{
    power_on(0); run_ms(100);
    tap(BTN_M_UP);
    for (int i = 0; i < 30; i++) tap(BTN_S_UP);
    run_ms(300);
    CHECK(ROW0_IS("SET  00:01:30"), "LCD shows SET 00:01:30");
    tap(BTN_START);
    uint32_t p = t_press;
    run_ms(10000);
    int s = lcd_seconds();
    CHECK(s >= 79 && s <= 81, "after 10 s the display reads ~00:01:20");
    uint32_t d = until_buzzer(p, 200000);
    printf("    info  START -> first beep: %u ms (ideal 90500)\n", (unsigned)d);
    CHECK(d >= 90500 && d <= 90560, "alarm fires after 90 s (+500 ms buzzer period)");
    run_ms(1100);
    CHECK(ROW0_IS("Time is up!"), "LCD shows Time is up!");
}

static void s_default_30min(void)
{
    power_on(0); run_ms(100);
    tap(BTN_START);
    uint32_t p = t_press;
    run_ms(2500);
    int s = lcd_seconds();
    printf("    info  display after 2.5 s reads %d s remaining\n", s);
    CHECK(s >= 1797 && s <= 1799, "default run shows ~00:29:58 (LCD may lag up to 1 s)");
    uint32_t d = until_buzzer(p, 2000000);
    printf("    info  START -> first beep: %u ms (ideal 1800500)\n", (unsigned)d);
    CHECK(d >= 1800500 && d <= 1800560, "default timer is 30 min with <60 ms error");
    CHECK(twin_wdt_max_gap() <= 2, "watchdog kicked every loop (max gap <= 2 ms)");
}

static void s_debounce(void)
{
    power_on(0); run_ms(100);
    for (int i = 0; i < 10; i++) { app_button_isr(BTN_H_UP); run_ms(2); }  /* contact bounce */
    run_ms(300);
    CHECK(ROW0_IS("SET  01:00:00"), "10 bounces in 20 ms count as ONE press");
}

static void s_wrap(void)
{
    power_on(0); run_ms(100);
    tap(BTN_H_DN); tap(BTN_M_DN); tap(BTN_S_DN); run_ms(300);
    CHECK(ROW0_IS("SET  23:59:59"), "all fields wrap 0 -> max on DOWN");
    tap(BTN_H_UP); tap(BTN_M_UP); tap(BTN_S_UP); run_ms(300);
    CHECK(ROW0_IS("SET  00:00:00"), "all fields wrap max -> 0 on UP");
}

static void s_lcd_rate(void)
{
    power_on(0); run_ms(100);
    tap(BTN_H_UP); run_ms(300);
    unsigned long w0 = twin_lcd_writes(); run_ms(2000);
    unsigned long w = twin_lcd_writes() - w0;
    printf("    info  LCD writes in 2 s while SETTING: %lu\n", w);
    CHECK(w >= 18 && w <= 22, "LCD refreshes at 200 ms while setting (10 rows/s)");
    tap(BTN_START); run_ms(1500);
    w0 = twin_lcd_writes(); run_ms(5000); w = twin_lcd_writes() - w0;
    printf("    info  LCD writes in 5 s while RUNNING: %lu\n", w);
    CHECK(w >= 8 && w <= 12, "LCD refreshes at 1 s while running (2 rows/s)");
}

static void s_locked_while_running(void)
{
    power_on(0); run_ms(100);
    for (int i = 0; i < 5; i++) tap(BTN_S_UP);
    tap(BTN_START); uint32_t p = t_press;
    tap(BTN_H_UP); tap(BTN_M_UP);
    uint32_t d = until_buzzer(p, 20000);
    CHECK(d >= 5500 && d <= 5560, "H+/M+ ignored while RUNNING (still 5 s)");
}

static void s_silence(void)
{
    power_on(0); run_ms(100);
    tap(BTN_S_UP); tap(BTN_S_UP); tap(BTN_START);
    until_buzzer(now, 10000);
    CHECK(twin_buzzer(), "buzzer sounds at zero");
    tap(BTN_H_UP);
    CHECK(!twin_buzzer(), "any button silences the alarm");
    unsigned long e = twin_buzzer_edges(); run_ms(3000);
    CHECK(twin_buzzer_edges() == e, "no more beeps after silencing");
    CHECK(ROW0_IS("Pomodoro Timer"), "returns to IDLE splash");
}

static void s_alarm_timeout(void)
{
    power_on(0); run_ms(100);
    tap(BTN_S_UP); tap(BTN_S_UP); tap(BTN_START);
    until_buzzer(now, 10000);
    run_ms(12000);
    CHECK(twin_buzzer_edges() == 20, "alarm = 20 toggles (10 beeps)");
    CHECK(!twin_buzzer(), "buzzer off after ~10 s with no button press");
    CHECK(ROW0_IS("Pomodoro Timer"), "auto-returns to IDLE");
}

static void s_millis_rollover(void)
{
    power_on(0xFFFFF000u);                       /* wraps after 4096 ms */
    run_ms(100);
    for (int i = 0; i < 10; i++) tap(BTN_S_UP);
    tap(BTN_START); uint32_t p = t_press;
    uint32_t d = until_buzzer(p, 20000);
    printf("    info  10 s timer across millis() wrap: %u ms (ideal 10500)\n", (unsigned)d);
    CHECK(d >= 10500 && d <= 10560, "timing survives 32-bit millis() overflow");
}

static void s_start_restarts(void)
{
    power_on(0); run_ms(100);
    for (int i = 0; i < 20; i++) tap(BTN_S_UP);
    tap(BTN_START); run_ms(10500);          /* press START again mid-second */
    tap(BTN_START); uint32_t p = t_press;
    uint32_t d = until_buzzer(p, 60000);
    printf("    info  OBSERVED: START while RUNNING restarts; 2nd START -> first beep = %u ms (ideal 20500)\n", (unsigned)d);
    CHECK(d >= 19500 && d <= 20560, "2nd START restarts from the set time (no pause/resume yet)");
    CHECK(d < 20450, "FINDING: counter phase not reset, so the 1st second after a restart is short");
}

typedef struct { const char *name; void (*fn)(void); } scenario_t;
static const scenario_t SC[] = {
    { "boot splash",                 s_boot },
    { "set 00:01:30 and run",        s_set_and_run },
    { "default 30-minute timer",     s_default_30min },
    { "button debounce",             s_debounce },
    { "field wrap-around",           s_wrap },
    { "LCD refresh rate",            s_lcd_rate },
    { "edits locked while running",  s_locked_while_running },
    { "silence alarm",               s_silence },
    { "alarm auto-timeout",          s_alarm_timeout },
    { "millis() rollover",           s_millis_rollover },
    { "START while running",         s_start_restarts },
};
#define NSC (sizeof SC / sizeof SC[0])

static int run_tests(void)
{
    int failed = 0;
    for (unsigned i = 0; i < NSC; i++) {
        printf("[%2u/%u] %s\n", i + 1, (unsigned)NSC, SC[i].name);
        fflush(stdout);
        pid_t pid = fork();                       /* fresh process = fresh power cycle */
        if (pid == 0) { g_fail = 0; SC[i].fn(); fflush(stdout); _exit(g_fail ? 1 : 0); }
        int st = 0; waitpid(pid, &st, 0);
        if (!WIFEXITED(st) || WEXITSTATUS(st)) failed++;
    }
    printf("\n%u scenarios, %d failed\n", (unsigned)NSC, failed);
    return failed ? 1 : 0;
}

/* ====================== interactive terminal twin ====================== */
static struct termios g_orig;
static int g_flags;
static void raw_off(void) { tcsetattr(0, TCSANOW, &g_orig); fcntl(0, F_SETFL, g_flags); printf("\033[?25h\n"); }
static void raw_on(void)
{
    tcgetattr(0, &g_orig);
    g_flags = fcntl(0, F_GETFL);
    fcntl(0, F_SETFL, g_flags | O_NONBLOCK);   /* never block the super loop */
    struct termios t = g_orig;
    t.c_lflag &= ~(tcflag_t)(ICANON | ECHO);
    t.c_cc[VMIN] = 0; t.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &t);
    atexit(raw_off);
    printf("\033[2J\033[?25l");
}

static double mono(void)
{
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void draw(double speed, bool beeping)
{
    uint32_t s = now / 1000;
    printf("\033[H");
    printf(" POMODORO DIGITAL TWIN   sim time %02u:%02u:%02u   speed %.0fx      \n", s / 3600, (s / 60) % 60, s % 60, speed);
    printf("\n  +----------------+\n  |%s|\n  |%s|\n  +----------------+\n", twin_lcd_row(0), twin_lcd_row(1));
    printf("\n  Buzzer: %s\n", beeping ? "[ BEEP BEEP ]" : "[ silent    ]");
    printf("\n  Buttons      H+ q   M+ w   S+ e      START = space\n");
    printf("               H- a   M- s   S- d      quit  = x\n");
    fflush(stdout);
}

static int run_interactive(double speed)
{
    raw_on();
    power_on(0);
    double t0 = mono(), last_draw = 0;
    bool prev_buzz = false;
    for (;;) {
        uint32_t target = (uint32_t)((mono() - t0) * 1000.0 * speed);
        while (now < target) tick();                 /* never skip a ms: faithful at any speed */
        char c;
        while (read(0, &c, 1) == 1) {
            switch (c) {
            case 'q': app_button_isr(BTN_H_UP);  break;
            case 'a': app_button_isr(BTN_H_DN);  break;
            case 'w': app_button_isr(BTN_M_UP);  break;
            case 's': app_button_isr(BTN_M_DN);  break;
            case 'e': app_button_isr(BTN_S_UP);  break;
            case 'd': app_button_isr(BTN_S_DN);  break;
            case ' ': case '\n': app_button_isr(BTN_START); break;
            case 'x': case 3: return 0;
            }
        }
        if (twin_buzzer() && !prev_buzz) { putchar('\a'); }
        prev_buzz = twin_buzzer();
        if (mono() - last_draw > 0.05) { draw(speed, twin_buzzer()); last_draw = mono(); }
        usleep(1000);
    }
}

int main(int argc, char **argv)
{
    double speed = 1.0; bool test = false;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--test")) test = true;
        else if (!strcmp(argv[i], "--speed") && i + 1 < argc) speed = atof(argv[++i]);
        else { fprintf(stderr, "usage: %s [--test] [--speed N]\n", argv[0]); return 2; }
    }
    if (speed < 1) speed = 1;
    return test ? run_tests() : run_interactive(speed);
}
