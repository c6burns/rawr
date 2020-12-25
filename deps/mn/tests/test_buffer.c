#include "mn/test_harness.h"
#include "mn/buffer.h"
#include "mn/buffer_pool.h"

MN_TEST_CASE_BEGIN(mn_buffer_ops)
	mn_buffer_pool_t pool;
	mn_buffer_t *buffer1 = NULL;
	mn_buffer_t *buffer2 = NULL;
	size_t blocks = 128;

	ASSERT_SUCCESS(mn_buffer_pool_setup(&pool, blocks, 8));
	mn_buffer_pool_cleanup(&pool);

	ASSERT_SUCCESS(mn_buffer_pool_setup(&pool, blocks, 8));
	for (size_t i = 0; i < blocks; i++) {
		ASSERT_SUCCESS(mn_buffer_pool_pop(&pool));
	}
	ASSERT_TRUE(0 != mn_buffer_pool_pop(&pool));
	mn_buffer_pool_cleanup(&pool);

	ASSERT_SUCCESS(mn_buffer_pool_setup(&pool, blocks, 8));
	ASSERT_SUCCESS(mn_buffer_pool_pop_back(&pool, &buffer1));
	ASSERT_TRUE(pool.blocks_inuse == 1);
	ASSERT_SUCCESS(mn_buffer_pool_peek(&pool, &buffer2));
	ASSERT_TRUE(pool.blocks_inuse == 1);
	ASSERT_SUCCESS(mn_buffer_pool_pop_back(&pool, &buffer2));
	ASSERT_TRUE(pool.blocks_inuse == 2);
	ASSERT_SUCCESS(mn_buffer_pool_push(&pool, buffer1));
	ASSERT_TRUE(pool.blocks_inuse == 1);
	ASSERT_SUCCESS(mn_buffer_release(buffer2));
	ASSERT_TRUE(pool.blocks_inuse == 0);
	ASSERT_TRUE(0 != mn_buffer_pool_push(&pool, buffer1));
	ASSERT_TRUE(pool.blocks_inuse == 0);
	ASSERT_TRUE(0 != mn_buffer_release(buffer2));
	ASSERT_TRUE(pool.blocks_inuse == 0);
	ASSERT_SUCCESS(0 != mn_buffer_pool_peek(&pool, &buffer1));
	ASSERT_TRUE(pool.blocks_inuse == 0);

	buffer1 = NULL;
	buffer2 = NULL;

	ASSERT_SUCCESS(mn_buffer_pool_pop_back(&pool, &buffer1));
	ASSERT_SUCCESS(mn_buffer_pool_pop_back(&pool, &buffer2));

	ASSERT_SUCCESS(mn_buffer_pool_push(&pool, buffer1));
	ASSERT_SUCCESS(mn_buffer_release(buffer2));
	mn_buffer_pool_cleanup(&pool);

	return MN_SUCCESS;
}

MN_TEST_CASE(mn_buffer_ops);
