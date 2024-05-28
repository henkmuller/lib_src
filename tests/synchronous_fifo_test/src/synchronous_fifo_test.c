// Copyright 2023-2024 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#include <xcore/parallel.h>
#include <xcore/hwtimer.h>
#include <xs1.h>
#include <stdlib.h>
#include <stdio.h>
#include <print.h>
#include <platform.h>
#include <xscope.h>
#include "synchronous_fifo.h"
#include "asrc_timestamp_interpolation.h"

#include <string.h>

#include "src.h"

#define SLOW 2000
#define FAST 1000

DECLARE_JOB(producer,   (synchronous_fifo_t *, int, int, int, int *));
DECLARE_JOB(consumer,   (synchronous_fifo_t *, int, int));
DECLARE_JOB(test_async, (int, int, int, int, int, int *));

#define seconds 10
#define OFFSET 0 // 0x70000000


void producer(synchronous_fifo_t *a, int producer_samples, int timestep, int *errors) {
    hwtimer_t tmr = hwtimer_alloc();
    int32_t now = hwtimer_get_time(tmr);
    for(int32_t i = 0; i < ...; i++) {
        now += timestep;
        hwtimer_set_trigger_time(tmr, now);
        (void) hwtimer_get_time(tmr);
        error = synchronous_fifo_producer_put(a, (int32_t *)out_samples);
    }
    hwtimer_free(tmr);
}

void consumer(synchronous_fifo_t *a, int consumer_samples, int timestep) {
    hwtimer_t tmr = hwtimer_alloc();
    int32_t now = hwtimer_get_time(tmr);
    for(int i = 0; i < ...; i++) {
        now += timestep;
        hwtimer_set_trigger_time(tmr, now);
        (void) hwtimer_get_time(tmr);
        synchronous_fifo_consumer_get(a, &output_data, now + OFFSET);
    }
    hwtimer_free(tmr);
}

#define FIFO_LENGTH   1024

void test_async(int producer_samples, int consumer_samples, int fifo_length,
                int ticks_p, int ticks_c,
                int *errors) {
    int64_t array[ASYNCHRONOUS_FIFO_INT64_ELEMENTS(FIFO_LENGTH, 1)];
    synchronous_fifo_t *synchronous_fifo_state = (synchronous_fifo_t *)array;

    int errors_p = 0;
    int errors_c = 0;
    synchronous_fifo_init(synchronous_fifo_state, 1,
                           producer_samples, consumer_samples,fifo_length,
                           0);

    PAR_JOBS(
        PJOB(producer, (synchronous_fifo_state, producer_samples, ticks_p, errors_p)),
        PJOB(consumer, (synchronous_fifo_state, consumer_samples, ticks_c, errors_c))
        );
    synchronous_fifo_exit(synchronous_fifo_state);
    *errors = errors_p + errors_c;
}

int test_slow_fast() {
    int e0=0, e1=0, e2=0, e3=0;
    printf("Testing all\n");
    PAR_JOBS(
        PJOB(test_async, (16, 256, 512, SLOW, FAST, &e0)),
        PJOB(test_async, (512, 32, 1024, SLOW, FAST, &e1)),
        PJOB(test_async, (256, 256, 256, SLOW, FAST, &e2)),
        PJOB(test_async, (256, 257, 768, SLOW, FAST, &e3))
        );
    return e0 + e1 + e2 + e3;
}

int test_fast_slow() {
    int e0=0, e1=0, e2=0, e3=0;
    printf("Testing all\n");
    PAR_JOBS(
        PJOB(test_async, (16, 256, 512,  FAST, SLOW, &e0)),
        PJOB(test_async, (512, 32, 1024, FAST, SLOW, &e1)),
        PJOB(test_async, (256, 256, 256, FAST, SLOW, &e2)),
        PJOB(test_async, (256, 257, 768, FAST, SLOW, &e3))
        );
    return e0 + e1 + e2 + e3;
}

int main(void) {
    int errors = 0;
    hwtimer_free_xc_timer();
    errors += test_slow_fast();
    errors += test_fast_slow();
    if (errors == 0) {
        printf("PASS\n");
    } else {
        printf("FAIL: %d errors\n", errors);
    }
    hwtimer_realloc_xc_timer();
    return errors;
}


