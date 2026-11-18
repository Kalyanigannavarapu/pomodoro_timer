#include "sched.h"
#include "hal.h"

static task_t *g_tasks;
static uint8_t g_count;

void sched_init(task_t *tasks, uint8_t count)
{
    g_tasks = tasks;
    g_count = count;
    uint32_t now = hal_millis();
    for (uint8_t i = 0; i < count; i++) g_tasks[i].last_ms = now;
}

void sched_enable(uint8_t id, bool enable)
{
    if (enable && !g_tasks[id].enabled) g_tasks[id].last_ms = hal_millis();
    g_tasks[id].enabled = enable;
}

void sched_set_period(uint8_t id, uint32_t period_ms)
{
    g_tasks[id].period_ms = period_ms;
}

void sched_run(void)
{
    uint32_t now = hal_millis();
    for (uint8_t i = 0; i < g_count; i++) {
        task_t *t = &g_tasks[i];
        if (!t->enabled) continue;
        uint32_t elapsed = now - t->last_ms;      /* safe on millis() overflow */
        if (elapsed >= t->period_ms) {
            /* keep absolute timing (no drift), resync if we fell far behind */
            t->last_ms = (elapsed >= 2 * t->period_ms) ? now : t->last_ms + t->period_ms;
            t->run();
        }
    }
}
