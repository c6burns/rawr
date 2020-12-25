#include "mn/test_harness.h"

#include "mn/error.h"
#include "mn/allocator.h"
#include "mn/log.h"
#include "mn/atomic.h"
#include "mn/thread.h"
#include "mn/queue_spsc.h"


static uint64_t queue_capacity = (1 << 25);// (1 << 10);
static uint64_t msgs_limit = 10000000;
static uint64_t err_read, err_write;
static uint64_t msgs_processed;
static int threads_quit = 0;
static mn_atomic_t test_ready;
static mn_atomic_t threads_ready;

void consume_spsc_thread_run(void *priv)
{
	uintptr_t prev;
	uint64_t i, count;
	mn_queue_spsc_t *queue = priv;

	const size_t capacity = 100000;
	const uint16_t block_size = sizeof(void *);
	const size_t capacity_bytes = block_size * capacity;

	uint64_t *num = MN_MEM_ACQUIRE(capacity_bytes);

	uint64_t ready = false;
	mn_atomic_fetch_add(&threads_ready, 1);
	while (!ready) {
		ready = mn_atomic_load(&test_ready);
	}

	err_read = prev = 0;
	while (ready) {
		count = capacity;
		if (mn_queue_spsc_pop_all(queue, (void **)num, &count)) {
			mn_thread_sleep_ms(16);
			err_read++;
			continue;
		}

		for (i = 0; i < count; i++) {
			if (num[i] != (prev + 1)) {
				mn_log_error("returned: %zu but %zu was expected", num[i], (prev + 1));
				ready = 0;
				break;
			}
			prev++;
			if (prev >= msgs_limit) {
				ready = 0;
				break;
			}
		}
	}

	MN_MEM_RELEASE(num);
}


void produce_spsc_thread_run(void *priv)
{
	uintptr_t num = 1;
	mn_queue_spsc_t *queue = priv;

	uint64_t ready = false;
	mn_atomic_fetch_add(&threads_ready, 1);
	while (!ready) {
		ready = mn_atomic_load(&test_ready);
	}

	err_write = 0;
	while (ready) {
		if (mn_queue_spsc_push(queue, (void *)num)) {
			err_write++;
			continue;
		}

		if (num >= msgs_limit) break;
		num++;
	}
}


MN_TEST_CASE_BEGIN(mn_queue_spsc_stress)
	mn_queue_spsc_t queue;
	mn_thread_t consume_thread, produce_thread;
	static uint64_t tstamp_start, tstamp_end;

	assert(((sizeof(queue) % MN_CACHE_LINE_SIZE) == 0));

	msgs_processed = 0;
	err_read = 0;
	err_write = 0;
	threads_quit = 0;

	mn_atomic_store(&test_ready, 0);
	mn_atomic_store(&threads_ready, 0);

	MN_GUARD_CLEANUP(mn_queue_spsc_setup(&queue, queue_capacity));
	MN_GUARD_CLEANUP(mn_thread_launch(&produce_thread, produce_spsc_thread_run, &queue));
	MN_GUARD_CLEANUP(mn_thread_launch(&consume_thread, consume_spsc_thread_run, &queue));

	while (mn_atomic_load(&threads_ready) < 2) {};
	mn_atomic_store(&test_ready, 1);

	tstamp_start = mn_tstamp();

	MN_GUARD_CLEANUP(mn_thread_join(&produce_thread));
	MN_GUARD_CLEANUP(mn_thread_join(&consume_thread));

	tstamp_end = mn_tstamp();

	mn_queue_spsc_cleanup(&queue);

	//uint64_t elapsed_ns = tstamp_end - tstamp_start;
	//double elapsed_s = (double)elapsed_ns / MN_TIME_NS_PER_S;
	//double msgs_per_second = (double)msgs_limit / elapsed_s;

	//mn_log_info("mn_queue_spsc test results ---------------------------------------");
	//mn_log_info("%zu intptrs transferred in %0.2f seconds: %0.2f per second", msgs_limit, elapsed_s, msgs_per_second);
	//mn_log_info("%zu read errors / %zu write errors", err_read, err_write);

	return MN_SUCCESS;

cleanup:
	return MN_ERROR;
}

MN_TEST_CASE_BEGIN(mn_queue_spsc_testempty)
	uintptr_t intptr;
	uintptr_t intptr2;
	mn_queue_spsc_t queue;

	ASSERT_SUCCESS(mn_queue_spsc_setup(&queue, 8));
	ASSERT_TRUE(MN_QUEUE_EMPTY == mn_queue_spsc_pop(&queue));
	ASSERT_TRUE(MN_QUEUE_EMPTY == mn_queue_spsc_peek(&queue, (void **)&intptr));
	ASSERT_NULL(intptr);
	ASSERT_TRUE(MN_QUEUE_EMPTY == mn_queue_spsc_pop_back(&queue, (void **)&intptr));
	ASSERT_NULL(intptr);

	intptr = 235235;
	intptr2 = 0;
	ASSERT_SUCCESS(mn_queue_spsc_push(&queue, (void *)intptr));
	ASSERT_SUCCESS(mn_queue_spsc_pop_back(&queue, (void **)&intptr2));
	ASSERT_TRUE(intptr2 == intptr);

	intptr = 34754678;
	intptr2 = 0;
	ASSERT_SUCCESS(mn_queue_spsc_push(&queue, (void *)intptr));
	ASSERT_SUCCESS(mn_queue_spsc_peek(&queue, (void **)&intptr2));
	ASSERT_TRUE(intptr2 == intptr);

	ASSERT_SUCCESS(mn_queue_spsc_pop(&queue));
	ASSERT_TRUE(MN_QUEUE_EMPTY == mn_queue_spsc_pop(&queue));
	ASSERT_TRUE(MN_QUEUE_EMPTY == mn_queue_spsc_peek(&queue, (void **)&intptr2));
	ASSERT_NULL(intptr2);
	ASSERT_TRUE(MN_QUEUE_EMPTY == mn_queue_spsc_pop_back(&queue, (void **)&intptr2));
	ASSERT_NULL(intptr2);

	return MN_SUCCESS;
}

MN_TEST_CASE_BEGIN(mn_queue_spsc_testfull)
	uintptr_t intptr = 97234;
	uintptr_t intptr2 = 83214;
	mn_queue_spsc_t queue;
	ASSERT_SUCCESS(mn_queue_spsc_setup(&queue, 8));

	for (int i = 0; i < 8; i++) {
		ASSERT_SUCCESS(mn_queue_spsc_push(&queue, (void *)intptr));
	}

	intptr2 = 0;
	ASSERT_TRUE(MN_QUEUE_FULL == mn_queue_spsc_push(&queue, (void *)intptr));
	ASSERT_SUCCESS(mn_queue_spsc_pop_back(&queue, (void **)&intptr2));
	ASSERT_TRUE(intptr2 == intptr);

	intptr2 = 0;
	ASSERT_SUCCESS(mn_queue_spsc_push(&queue, (void *)intptr));
	ASSERT_TRUE(MN_QUEUE_FULL == mn_queue_spsc_push(&queue, (void *)intptr));
	ASSERT_SUCCESS(mn_queue_spsc_peek(&queue, (void **)&intptr2));
	ASSERT_TRUE(intptr2 == intptr);

	ASSERT_TRUE(MN_QUEUE_FULL == mn_queue_spsc_push(&queue, (void *)intptr));

	return MN_SUCCESS;
}

MN_TEST_CASE_BEGIN(mn_queue_spsc_npot)
	mn_queue_spsc_t queue;
	ASSERT_SUCCESS(MN_ERROR_INVAL == mn_queue_spsc_setup(&queue, 11));
	ASSERT_TRUE(16 == mn_queue_spsc_capacity(&queue));
	mn_queue_spsc_cleanup(&queue);

	ASSERT_SUCCESS(mn_queue_spsc_setup(&queue, 16));
	ASSERT_TRUE(16 == mn_queue_spsc_capacity(&queue));
	mn_queue_spsc_cleanup(&queue);

	ASSERT_SUCCESS(mn_queue_spsc_setup(&queue, 17));
	ASSERT_TRUE(32 == mn_queue_spsc_capacity(&queue));
	mn_queue_spsc_cleanup(&queue);

	return MN_SUCCESS;
}

MN_TEST_CASE(mn_queue_spsc_stress);
MN_TEST_CASE(mn_queue_spsc_testempty);
MN_TEST_CASE(mn_queue_spsc_testfull);
MN_TEST_CASE(mn_queue_spsc_npot);
