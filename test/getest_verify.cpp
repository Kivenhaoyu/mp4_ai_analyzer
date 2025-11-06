//
//  getest_verify.cpp
//  mp4_ai_analyzer
//
//  Created by Elena Aaron on 5/11/2025.
//

#include <stdio.h>

#include <gtest.h>

// 测试套件：验证GTest基础功能
TEST(GTestVerify, BasicAssertions) {
    // 断言：2+2=4（应通过）
    EXPECT_EQ(2 + 2, 4);
    // 断言：条件为真（应通过）
    EXPECT_TRUE(true);
    // （可选）故意写一个失败断言，验证错误提示
    // EXPECT_EQ(1, 2); // 取消注释后会触发失败
}

// 主函数：启动GTest测试
int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
