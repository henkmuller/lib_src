// Copyright 2023-2024 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#include <xcore/parallel.h>
#include <xcore/hwtimer.h>
#include <xs1.h>
#include <stdlib.h>
#include <stdio.h>
#include "synchronous_fifo.h"
#include "asrc_timestamp_interpolation.h"

#include <string.h>
#include <assert.h>

#include "src.h"

#define SLOW 200
#define FAST 100

DECLARE_JOB(producer,   (synchronous_fifo_t *, int, int, int *));
DECLARE_JOB(consumer,   (synchronous_fifo_t *, int, int, int *));
DECLARE_JOB(test_async, (int, int, int, int, int, int *));

#define MAX_FIFO_LENGTH   1024
#define CHANNELS             2
#define RUN_SAMPLES       2048
#define OFFSET            0x10000000

void producer(synchronous_fifo_t *a, int producer_samples, int timestep, int *errors) {
    hwtimer_t tmr = hwtimer_alloc();
    int32_t now = hwtimer_get_time(tmr);
    int rt;
    for(int i = 0; i < RUN_SAMPLES; i+=producer_samples) {
        int32_t out_samples[MAX_FIFO_LENGTH][CHANNELS];
        asm volatile ("gettime %0" : "=r" (rt));
        for(int j = 0; j < producer_samples; j++) {
            out_samples[j][0] = (i + j) * (i+j);
            out_samples[j][1] = rt+OFFSET;
        }
        now += timestep * producer_samples;
        hwtimer_set_trigger_time(tmr, now);
        now = hwtimer_get_time(tmr);   // skid on purpose
        synchronous_fifo_producer_put(a, &out_samples[0][0]);
//        printf("P %d\n", i);
    }
    hwtimer_free(tmr);
}

void consumer(synchronous_fifo_t *a, int consumer_samples, int timestep, int *errors) {
    hwtimer_t tmr = hwtimer_alloc();
    int32_t now = hwtimer_get_time(tmr);
    int delays[RUN_SAMPLES][2];
    int rt;
    for(int i = 0; i < RUN_SAMPLES; i+=consumer_samples) {
        int32_t input_samples[MAX_FIFO_LENGTH][CHANNELS];
        synchronous_fifo_consumer_get(a, &input_samples[0][0]);
        asm volatile ("gettime %0" : "=r" (rt));
        for(int j = 0; j < consumer_samples; j++) {
            int expected = (i+j) * (i+j);
            if (input_samples[j][0] != expected) {
                printf("Error, sample %d %d should be %d\n", j, (int)input_samples[j], expected);
                (*errors)++;
            }
            delays[i+j][0] = input_samples[j][1] - OFFSET;
            delays[i+j][1] = rt;
        }
        now += timestep * consumer_samples;
        hwtimer_set_trigger_time(tmr, now);
        now = hwtimer_get_time(tmr);   // skid on purpose
//        printf("C %d\n", i);
    }
    for(int i = 0; i < RUN_SAMPLES; i += consumer_samples) {
        printf("%d tx %d rx %d delay %d\n", i, delays[i][0], delays[i][1], delays[i][1] - delays[i][0]);
    }
    hwtimer_free(tmr);
}

void test_async(int producer_samples, int consumer_samples, int fifo_length,
                int ticks_p, int ticks_c,
                int *errors) {
    int64_t array[SYNCHRONOUS_FIFO_INT64_ELEMENTS(MAX_FIFO_LENGTH, CHANNELS)];
    synchronous_fifo_t *synchronous_fifo_state = (synchronous_fifo_t *)array;

    assert(fifo_length <= MAX_FIFO_LENGTH);
    int errors_p = 0;
    int errors_c = 0;
    channel_t chans = chan_alloc();
    synchronous_fifo_init(synchronous_fifo_state, chans, CHANNELS,
                           producer_samples, consumer_samples,fifo_length);

    PAR_JOBS(
        PJOB(producer, (synchronous_fifo_state, producer_samples, ticks_p, &errors_p)),
        PJOB(consumer, (synchronous_fifo_state, consumer_samples, ticks_c, &errors_c))
        );
    synchronous_fifo_exit(synchronous_fifo_state);
    *errors = errors_p + errors_c;
    printf("%3d %3d %4d  %5d %5d done\n", producer_samples, consumer_samples,
           fifo_length, ticks_p * producer_samples, ticks_c * consumer_samples);
    chan_free(chans);
}

int test_slow_fast() {
    int e0=0, e1=0, e2=0, e3=0;
    printf("Testing slow fast\n");
    PAR_JOBS(
        PJOB(test_async, ( 16, 256,  512, SLOW, FAST, &e0)),
        PJOB(test_async, (512,  32, 1024, SLOW, FAST, &e1)),
        PJOB(test_async, (256, 256,  256, SLOW, FAST, &e2)),
        PJOB(test_async, (256, 256,  768, SLOW, FAST, &e3))
        );
    return e0 + e1 + e2 + e3;
}

int test_fast_slow() {
    int e0=0, e1=0, e2=0, e3=0;
    printf("Testing fast slow\n");
    PAR_JOBS(
        PJOB(test_async, ( 16, 256,  512, FAST, SLOW, &e0)),
        PJOB(test_async, (512,  32, 1024, FAST, SLOW, &e1)),
        PJOB(test_async, (256, 256,  256, FAST, SLOW, &e2)),
        PJOB(test_async, (256, 256,  768, FAST, SLOW, &e3))
        );
    return e0 + e1 + e2 + e3;
}

int test_short_queue() {
    int e0=0, e1=0, e2=0, e3=0;
    printf("Testing short queue\n");
    PAR_JOBS(
//        PJOB(test_async, ( 16, 256,  272, SLOW, FAST, &e0))
        PJOB(test_async, (256,  16,  272, SLOW, FAST, &e1))
//        PJOB(test_async, ( 16, 256,  272, FAST, SLOW, &e2))
//        PJOB(test_async, (256,  16,  272, FAST, SLOW, &e3))
        );
    return e0 + e1 + e2 + e3;
}

int main(void) {
    int errors = 0;
    hwtimer_free_xc_timer();
//    errors += test_slow_fast();
//    errors += test_fast_slow();
    errors += test_short_queue();
    if (errors == 0) {
        printf("PASS\n");
    } else {
        printf("FAIL: %d errors\n", errors);
    }
    hwtimer_realloc_xc_timer();
    return errors;
}


