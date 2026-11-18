#ifndef SCHED_H
#define SCHED_H
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    void     (*run)(void);
    uint32_t period_ms;
    uint32_t last_ms;
    bool     enabled;
} task_t;

void sched_init(task_t *tasks, uint8_t count);
void sched_enable(uint8_t id, bool enable);
void sched_set_period(uint8_t id, uint32_t period_ms);
void sched_run(void);
#endif
