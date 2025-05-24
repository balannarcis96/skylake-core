#include <skl_status>

#include <gtest/gtest.h>

TEST(SkylakeStatus, General) {

    ASSERT_FALSE(skl_status{SKL_ERR_PARAMS}.to_bool());
    ASSERT_TRUE(skl_status{SKL_SUCCESS}.to_bool());
    ASSERT_TRUE(skl_status{SKL_OK_REDUNDANT}.to_bool());

    const auto* str = skl_status{SKL_ERR_PARAMS}.to_string();
    ASSERT_EQ(0, strcmp(str, "SKL_ERR_PARAMS"));
    puts(str);

    str = skl_status{12512}.to_string();
    ASSERT_EQ(0, strcmp(str, "[No enum entry for skl_status=12512]"));
    puts(str);
}
