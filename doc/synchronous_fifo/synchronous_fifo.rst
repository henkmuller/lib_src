Synchronous FIFO
=================

A synchronous FIFO is a blocking data structure in which elements
gets pushed in on one side and pulled out on the other side.
It is primarily designed to be used with between DSP elements to help build
systems where the block-rate is changing between DSP elements.

Two typical use cases are shown in :ref:`synchronous_FIFO_use_cases`.
In the first use case we produce small batches of samples, but large
batches of samples are consumed; in the secon example we produce large
batches but we consume small batches.

.. _synchronous_FIFO_use_cases:

.. figure:: images/use-cases.*
            :width: 75%

            Use cases for the synchronous FIFO

In order to use the synchronous FIFO one needs at least two threads that
are located on the same tile. A producer thread (on the left), and a
consumer thread (on the right). These threads are free-running relative to
each other, but are at a high level in lock-step with each other.
The FIFO transports data from the producer to the consumer.
Free-running means that the threads can simultaneously access the FIFO
without being able to observe a change in timing, except for when the
producer is too fast (it will wait for the consumer), or when the consumer
is too fast (it will wait for the producer).

The FIFO has a fixed length, set on creation.


Using the Synchronous FIFO
---------------------------

A synchronous FIFO is allocated as an array of double-word integers::

  int64_t array[SYNCHRONOUS_FIFO_INT64_ELEMENTS(ENTRIES, SAMPLE_SIZE)];

The ``SYNCHRONOUS_FIFO_INT64_ELEMENTS()`` macro calculates the number of
double words required for the FIFO given the number of entries in the FIFO,
and the number of words that each sample occupies. For example, when
transferring stereo Audio through a fifo with 40 elements one would use
``SYNCHRONOUS_FIFO_INT64_ELEMENTS(40, 2)``. The number 40 is the total number of elements in the
FIFO. The FIFO will start empty; if desired, it can be started to have one
quantum (defined later) inside it if desirable.

The number of elements in the FIFO is a trade-off that the system designer
makes. As the FIFO will always aim to be half-full, a large number of
elements will introduce a high latency in the system and occupy a large
amount of memory. A short FIFO wil contribute little latency but may easily
overflow and underflow.

The Synchronous FIFO has the following functions to control the FIFO:

* ``synchronous_fifo_init()`` initialises the FIFO structure. It needs to
  know the number of integers that comprise a single sample, the maximum
  length that has been allocated for the FIFO.

* ``synchronous_fifo_exit()`` uninitialises the FIFO structure.

* ``synchronous_fifo_producer_put()`` puts samples into the FIFO.

* ``synchronous_fifo_consumer_get()`` gets samples from the FIFO.

The FIFO operates on fixed, configurable, block sizes. On initialisation
these two block sizes are declared:

* ``producer_samples`` is the number of samples that is produced at any one
  time.

* ``consumer_samples`` is the number of samples that is consumed at any one
  time.

If ``consumer_samples`` is a multiple of ``producer_samples`` then the FIFO
makes larger blocks of data. For example, a DSP task may be making blocks
of 16, but another task may require blocks of 512. The FIFO will gather 32
blocks of 32 samples before passing them onto the producer in this case.

If ``producer_samples`` is a multiple of ``consumer_samples`` then the
FIFO chops large blocks of data into smaller blocks. For example, a DSP
task may produce 256 samples at a time, the next task may consume 32
samples at a time, and each block of 256 will be chopped into eight blocks
of 32.

It is legal for both sizes to be the same, but one must be a whole
multiple of the other. If not, the initialisation function will fail.

The largest of ``consumer_samples`` and ``producer_samples`` is called the
quantum; it is the size at which the producer and consumer synchronise.
Even though one side splits the quantum up, the system as a whole only
synchronises on whole quanta only. THe FIFO can contain at most eight
quanta.

API
---

.. doxygengroup:: src_fifo
   :content-only:


Internal workings of the Synchronous FIFO
------------------------------------------

This appendix details the inner workings of the FIFO and is intended only
for advanced users who wish to understand the operation in more detail.


Implementation of synchronicity
++++++++++++++++++++++++++++++++

The FIFO straddles two threads; this is essential as the two threads
operate on different heart-beats. Hence, the FIFO is a shared-memory
element between those two threads. A read-pointer (managed
by the consuming thread) and a write-pointer (managed by the producing thread)
are maintained independently. The consumer has a number of
quantum-credits on which the consumer can operate without blocking; the
producer has a number of quantum credits which an be pushed into the FIFO
before it blocks. During normal operation there is a small number of
credits on either side that allow both parts to work in parallel; but given
that one side is likely to run faster than the other part, eventually one
of the credits will reach zero and the threads will fall into lock-step
with the speed being limited by either consumer or producer.

Without loss of generality we define the Quantum to be the least common
multiple of ``producer_samples`` and ``consumer_samples``. (Indeed, one can
relax the requirement that one must be a multiple of the other and use the
LCM instead; however, where one is a multiple of the other, the LCM is the
highest of the two).

We use two channel-ends to, asynchronously, exchange credit tokens. Credit
tokens comprise the ``1`` control token and have a value of one quantum.
The producer will send one credit token to the consumer to indicate that one
quantum is available in the FIFO to be read out. The consumer will send one
credit token to the producer to indicate that one quantum of samples has
been consumed, freeing up space in the FIFO for one quantum of new samples.
As the credit tokens are sent asynchronously, they can build up in the
channel-end in a non-blocking manner.

This way, the producer and consumer are not aware of the precise length of
the FIFO, or indeed the position of the other pointer. This knowledge is
embodied in the credit tokens. Credit tokens are only accepted when
necessary, ie, when it appears that the FIFO may be empty the consumer will wait for
another credit token, and when the FIFO appears to be full the producer
will wait for another credit token.

As neither side may be operating on whole quanta, they will both count
samples within a quantum.

