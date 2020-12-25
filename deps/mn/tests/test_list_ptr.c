#include "mn/test_harness.h"

#include "mn/list_ptr.h"

#include <string.h>


MN_TEST_CASE_BEGIN(mn_list_ptr_create)
	mn_list_ptr_t list;

	ASSERT_SUCCESS(mn_list_ptr_setup(&list, 128));
	ASSERT_TRUE(list.capacity == 128);
	ASSERT_TRUE(list.index == 0);
	ASSERT_TRUE(mn_list_ptr_count(&list) == 0);
	ASSERT_NOT_NULL(list.data);
	mn_list_ptr_cleanup(&list);

	ASSERT_SUCCESS(mn_list_ptr_setup(&list, 8));
	ASSERT_TRUE(list.capacity == 8);
	ASSERT_TRUE(list.index == 0);
	ASSERT_TRUE(mn_list_ptr_count(&list) == 0);
	ASSERT_NOT_NULL(list.data);
	mn_list_ptr_cleanup(&list);

	return MN_SUCCESS;
}

MN_TEST_CASE_BEGIN(mn_list_ptr_ops)
	mn_list_ptr_t list;
	uint64_t data0 = 12983712837;
	uint64_t data1 = 97829736300;
	uint64_t *p0 = &data0;
	uint64_t *p1 = &data1;
	uint64_t *out = NULL;

	ASSERT_SUCCESS(mn_list_ptr_setup(&list, 128));
	ASSERT_TRUE(list.capacity == 128);
	ASSERT_TRUE(list.index == 0);
	ASSERT_TRUE(mn_list_ptr_count(&list) == 0);
	ASSERT_NOT_NULL(list.data);

	ASSERT_SUCCESS(mn_list_ptr_push_back(&list, p0));
	ASSERT_TRUE(mn_list_ptr_count(&list) == 1);

	ASSERT_SUCCESS(mn_list_ptr_push_back(&list, p1));
	ASSERT_TRUE(mn_list_ptr_count(&list) == 2);

	ASSERT_SUCCESS(mn_list_ptr_remove(&list, 0));
	ASSERT_TRUE(mn_list_ptr_count(&list) == 1);

	ASSERT_SUCCESS(mn_list_ptr_pop_back(&list, (void **)&out));
	ASSERT_TRUE(mn_list_ptr_count(&list) == 0);
	ASSERT_TRUE(out == p1);
	ASSERT_TRUE(*out == data1);

	ASSERT_FAILS(mn_list_ptr_pop_back(&list, (void **)&out));

	mn_list_ptr_cleanup(&list);

	return MN_SUCCESS;
}

MN_TEST_CASE(mn_list_ptr_create);
MN_TEST_CASE(mn_list_ptr_ops);
