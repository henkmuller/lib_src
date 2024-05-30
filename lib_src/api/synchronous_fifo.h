// Copyright 2023-2024 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#ifndef _synchronous_fifo_h__
#define _synchronous_fifo_h__

#include <stdint.h>
#include <xccompat.h>
#include <xcore/channel.h>
#include <xcore/chanend.h>

#define FREQUENCY_RATIO_EXPONENT     (32)

#ifdef __XC__
#define UNSAFE unsafe
#else
#define UNSAFE
#endif

/**
 * \addtogroup src_fifo src_fifo
 *
 * The public API for using Synchronous FIFO.
 * @{
 */


/**
 * Data structure that holds the status of an synchronous FIFO
 * Credits are recorded on both sides:
 *
 * - consumer_get_credit records the number of quantas that the consumer
 *   can take out of the FIFO before it must wait for credits from the 
 *   producer.
 *   consumer_partial_get_credit records the number of samples that can
 *   be taken out in addition to the whole number of quantas. This is for
 *   the case where consumer_samples < producer_samples
 *
 * - producer_put_credit records the number of quantas that the producer
 *   can put into the FIFO before it must wait for credits from the 
 *   consumer.
 *   producer_partial_get_credit records the number of samples that can
 *   be put in in addition to the whole number of quantas. This is for
 *   the case where producer_samples < consumer_samples
 * 
 * Credits are awared through passing an END-OF-MESSAGE token to the other side.
 * Each token is equivalent to one quantum worth of samples.
 *
 * The consumer and producer each have their own channel-end for receiving credits;
 * they are connected to each other so each can use them to send credits too.
 *
 */
typedef struct synchronous_fifo_t {
    // Updated on initialisation only
    int32_t   channel_count;                  /* Number of audio channels */
    int32_t   copy_mask;                      /* Number of audio channels */
    int32_t   max_fifo_depth;                 /* Length of buffer[] in channel_counts */
    uint32_t  credit_samples_quantum;         /* Number of samples for each credit token */
    uint32_t  consumer_samples;               /* Number of samples got on each consumer call */
    uint32_t  producer_samples;               /* Number of samples put on each producer call */

    // Updated on the producer side only
    int32_t   write_ptr;                      /* Write index in the buffer */
    int32_t   producer_put_credit;            /* Number of spaces on producer */
    chanend_t producer_chanend;               /* Channel end to sycnhronise on */
    int32_t   producer_get_credit;            /* Producer copy of consumer samples */

    // Updated on the consumer side only
    uint32_t  read_ptr;                       /* Read index in the buffer */
    int32_t   consumer_get_credit;            /* Number of samples on consumer */
    chanend_t consumer_chanend;               /* Channel end to sycnhronise on */
    int32_t   consumer_put_credit;            /* Consumer copy of producer spaces */

    // Updated from both sides
    int32_t   buffer[0];                      /* Buffer of data */
} synchronous_fifo_t;


/**
 * Function that must be called to initialise the synchronous FIFO.
 * The ``state`` argument should be an int64_t array of
 * ``SYNCHRONOUS_FIFO_INT64_ELEMENTS`` elements that is cast to
 * ``synchronous_fifo_t*``.
 * 
 * That pointer should also be used for all other operations, including
 * operations both the consumer and producer sides.
 *
 * The number of samples that are being produced and consumed are static
 * and set at initialisation time. The producer will put in
 * ``sizeof(int32_t) x channel_count x producer_samples`` bytes on each
 * iteration, and the consumer will get
 * ``sizeof(int32_t) x channel_count x consumer_samples`` bytes on each
 * iteration.
 *
 * producer_samples should divide consumer_samples, or consumer_samples
 * should divide producer_samples. In other words
 * GCD(producer_samples, consumer_samples) = MIN(producer_samples, consumer_samples)
 *
 * @param   state               Synchronous FIFO to be initialised
 *
 * @param   chan                A channel that this synchronous FIFO shall use.
 *
 * @param   channel_count       Number of audio channels
 * 
 * @param   producer_samples    Number of samples the producer will put in
 *                              on each call to synchronous_fifo_producer_put()
 * 
 * @param   consumer_samples    Number of samples the consumer will get in
 *                              on each call to synchronous_fifo_consumer_get()
 *
 * @param   max_fifo_depth      Length of the FIFO; it governs the maximum level
 *                              of decoupling
 */
void synchronous_fifo_init(synchronous_fifo_t * UNSAFE state,
                           channel_t chan,
                           int channel_count,
                           int producer_samples,
                           int consumer_samples,
                           int max_fifo_depth);

/**
 * Function that must be called to deinitalise the synchronous FIFO.
 * After calling this, you must free the channel.
 *
 * @param   state                   ASRC structure to be de-initialised
 */
void synchronous_fifo_exit(synchronous_fifo_t * UNSAFE state);

/**
 * Function that provides the next samples to the synchronous FIFO.
 *
 * This function and synchronous_fifo_consumer_get() function both need a timestamp,
 * which is the time that the last sample was input (this function) or
 * output (synchronous_fifo_consumer_get()). The synchronous FIFO will hand the
 * samples across from producer to consumer through an elastic queue, and run
 * a PID algorithm to calculate the best way to equalise the input clock relative
 * to the output clock. Therefore, the timestamps
 * have to be measured on either the same clock or two very similar clocks.
 * It is probably fine to use the reference clocks on two tiles, provided
 * the tiles came out of reset at more or less the same time. Using the
 * clocks from two different chips would require the two chips to share an
 * oscillator, and for them to come out of reset simultaneously.
 *
 * @param   state               ASRC structure to push the sample into
 *
 * @param   samples             The sample values.
 */
void synchronous_fifo_producer_put(synchronous_fifo_t * UNSAFE state,
                                   int32_t * UNSAFE samples);

/**
 * Function that gets an output sample from the synchronous FIFO
 *
 * @param   state               ASRC structure to read a sample out off.
 *
 * @param   samples             The array where the frame with output
 *                              samples will be stored.
 */
void synchronous_fifo_consumer_get(synchronous_fifo_t * UNSAFE state,
                                    int32_t * UNSAFE samples);


/**
 * macro that calculates the number of int64_t to be allocated for the fifo
 * for a FIFO of N elements and C channels
 */
#define SYNCHRONOUS_FIFO_INT64_ELEMENTS(N, C) (sizeof(synchronous_fifo_t)/sizeof(int64_t) + (N*C)/2+1)
#endif

/**@}*/ // END: addtogroup src_fifo
