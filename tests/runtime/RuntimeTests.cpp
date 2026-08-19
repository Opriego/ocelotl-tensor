#include "ocelotl/runtime/v1/runtime.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

TEST(RuntimeTest, AllocatesAlignedStorage) {
  void *pointer = ocelotl_rt_v1_alloc(257, 64);
  ASSERT_NE(pointer, nullptr) << ocelotl_rt_v1_last_error();
  EXPECT_EQ(reinterpret_cast<std::uintptr_t>(pointer) % 64, 0U);
  EXPECT_STREQ(ocelotl_rt_v1_last_error(), "");
  ocelotl_rt_v1_free(pointer);
}

TEST(RuntimeTest, RejectsInvalidAlignmentWithDiagnostic) {
  EXPECT_EQ(ocelotl_rt_v1_alloc(64, 3), nullptr);
  EXPECT_NE(std::string{ocelotl_rt_v1_last_error()}.find("power of two"),
            std::string::npos);
}

TEST(RuntimeTest, RejectsZeroSizeAndAcceptsNullFree) {
  EXPECT_EQ(ocelotl_rt_v1_alloc(0, 64), nullptr);
  EXPECT_NE(std::string{ocelotl_rt_v1_last_error()}.find("greater than zero"),
            std::string::npos);
  ocelotl_rt_v1_free(nullptr);
}
