// Copyright 2023-2024 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#include <string.h>
#include <assert.h>
#include "synchronous_fifo.h"
#include <xcore/select.h>

#include <xs1.h>
#include <stdio.h>
#include <stdlib.h>

void synchronous_fifo_init(synchronous_fifo_t *state,
                           channel_t c,
                           int channel_count,
                           int producer_samples,
                           int consumer_samples,
                           int max_fifo_depth) {
    state->consumer_chanend = c.end_a;
    state->producer_chanend = c.end_b;
    state->max_fifo_depth = max_fifo_depth;
    state->channel_count = channel_count;
    state->copy_mask     = (1 << (4*channel_count)) - 1;
    state->consumer_samples = consumer_samples;
    state->producer_samples = producer_samples;
    if (producer_samples % consumer_samples == 0) {
        state->credit_samples_quantum = producer_samples;
        assert(max_fifo_depth % consumer_samples == 0);
    } else if (consumer_samples % producer_samples == 0) {
        state->credit_samples_quantum = consumer_samples;
        assert(max_fifo_depth % producer_samples == 0);
    } else {
        // Optionally - set quantum to LCM of producer_samples and consumer_samples
        // Instead for now we just fail.
        assert(0);
    }
    // First initialise shared variables, or those that shouldn't reset on a RESET.
    state->write_ptr           = 0;
    state->producer_put_credit = state->credit_samples_quantum;
    state->producer_get_credit = 0;
    state->read_ptr            = 0;
    state->consumer_get_credit = 0;
    state->consumer_put_credit = max_fifo_depth % state->credit_samples_quantum;
    // Now clear the buffer.
    memset(state->buffer, 0, channel_count * max_fifo_depth * sizeof(int));
}

void synchronous_fifo_exit(synchronous_fifo_t *state) {
    SELECT_RES(CASE_THEN(state->producer_chanend, in_producer),
               CASE_THEN(state->consumer_chanend, in_consumer),
               DEFAULT_THEN(done)) {
    in_producer:
        chanend_check_control_token(state->producer_chanend, XS1_CT_END);
        continue;
    in_consumer:
        chanend_check_control_token(state->consumer_chanend, XS1_CT_END);
        continue;
    done:
        return;
    }
}

void synchronous_fifo_producer_put(synchronous_fifo_t *state, int32_t *samples) {
    int max_fifo_depth = state->max_fifo_depth;
    int write_ptr = state->write_ptr;
    int channel_count = state->channel_count;
    int copy_mask = state->copy_mask;
    state->producer_put_credit -= state->producer_samples;
    if (state->producer_put_credit < 0) {
        int t0, t1;
        asm volatile ("gettime %0" : "=r" (t0));
        chanend_check_control_token(state->producer_chanend, XS1_CT_END); // Wait for put credit
        asm volatile ("gettime %0" : "=r" (t1));
        state->producer_put_credit += state->credit_samples_quantum;
        printf("Tx wt %d %d -> %d [%d ~ %d]\n", t0, t1, t1-t0, state->producer_put_credit, state->consumer_put_credit);
    }
    for(int j = 0; j < state->producer_samples; j++) {
#ifdef __XS2A__
        memcpy(state->buffer + write_ptr * channel_count, samples, channel_count * sizeof(int));
        (void)copy_mask; // Remove unused var warning
#else
        register int32_t *ptr asm("r11") = samples;
        asm("vldr %0[0]" :: "r" (ptr));
        asm("vstrpv %0[0], %1" :: "r" (state->buffer + write_ptr * channel_count), "r" (copy_mask));
#endif
        write_ptr = (write_ptr + 1);
        if (write_ptr >= max_fifo_depth) {
            write_ptr = 0;
        }
        samples += channel_count;
    }
    state->write_ptr = write_ptr;
    // Now record samples for the consumer to get; and if a whole quantum is available send it across and record we've done so
    state->producer_get_credit += state->producer_samples;
    if (state->producer_get_credit >= state->credit_samples_quantum) {
        chanend_out_control_token(state->producer_chanend, XS1_CT_END); // Send get credit
        state->producer_get_credit -= state->credit_samples_quantum;
    }
}

void synchronous_fifo_consumer_get(synchronous_fifo_t *state, int32_t *samples) {
    int max_fifo_depth = state->max_fifo_depth;
    int read_ptr = state->read_ptr;
    int channel_count = state->channel_count;
    int copy_mask = state->copy_mask;
    int consumer_get_credit = state->consumer_get_credit - state->consumer_samples;
    if (consumer_get_credit < 0) {
        int t0, t1;
        asm volatile ("gettime %0" : "=r" (t0));
        chanend_check_control_token(state->consumer_chanend, XS1_CT_END); // Wait for credit
        asm volatile ("gettime %0" : "=r" (t1));
        consumer_get_credit += state->credit_samples_quantum;
        printf("Rx wt %d %d -> %d [%d ~ %d]\n", t0, t1, t1-t0, consumer_get_credit, state->producer_get_credit);
    }
    state->consumer_get_credit = consumer_get_credit;
    for(int j = 0; j < state->consumer_samples; j++) {
#ifdef __XS2A__
        memcpy(samples, state->buffer + read_ptr * channel_count, channel_count * sizeof(int));
        (void)copy_mask; // Remove unused var warning
#else
        register int32_t *ptr asm("r11") = state->buffer + read_ptr * channel_count;
        asm("vldr %0[0]" :: "r" (ptr));
        asm("vstrpv %0[0], %1" :: "r" (samples), "r" (copy_mask));
#endif
        read_ptr = (read_ptr + 1) % max_fifo_depth;
        samples += channel_count;
    }
    state->read_ptr = read_ptr;
    // Now record space for the producer to put; and if a whole quantum is available send it across and record we've done so
    state->consumer_put_credit += state->consumer_samples;
    if (state->consumer_put_credit >= state->credit_samples_quantum) {
        chanend_out_control_token(state->consumer_chanend, XS1_CT_END); // Send put credit
        state->consumer_put_credit -= state->credit_samples_quantum;
    }
}
