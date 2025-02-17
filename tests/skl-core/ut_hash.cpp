#include <gtest/gtest.h>

#include <skl_hash>
#include <skl_int>
#include <skl_rand>

TEST(SKLCoreHashTests, siphash_16) {
    skl::SklRand rand{};

    for (auto round = 0; round < 32; ++round) {
        printf("round: %d\n", round);
        byte source_buffer[16];
        byte source_key[16];

        //Rand buffer
        printf("\tb : ");
        for (auto& _b : source_buffer) {
            _b = rand.next_range(1U, 255U);
            printf("%X ", int(_b));
        }
        printf("\n");
        
        //Rand key
        printf("\tk : ");
        for (auto& _b : source_key) {
            _b = rand.next_range(1U, 255U);
            printf("%X ", int(_b));
        }
        printf("\n");

        byte output_1[16];
        byte output_2[16];

        memset(output_1, 0, 16);
        memset(output_2, 0, 16);

        skl::skl_siphash_16(source_buffer, source_key, output_1);
        skl::skl_siphash_16(source_buffer, source_key, output_2);

        printf("\to1: ");
        for (auto _b : output_1) {
            printf("%X ", int(_b));
        }
        printf("\n");

        printf("\to2: ");
        for (auto _b : output_2) {
            printf("%X ", int(_b));
        }
        printf("\n");

        for (auto i = 0; i < 16; ++i) {
            ASSERT_EQ(output_1[i], output_2[i]);
        }

        for (auto& _b : source_key) {
            _b = rand.next_range(1U, 255U);
            printf("%X ", int(_b));
        }
        skl::skl_siphash_16(source_buffer, source_key, output_2);

        i32 eq_count = 0;
        for (auto i = 0; i < 16; ++i) {
            eq_count += i32(output_1[i] == output_2[i]);
        }
        ASSERT_TRUE(16 / 2 > eq_count);

        puts("");
    }
}

TEST(SKLCoreHashTests, siphash_8) {
    skl::SklRand rand{};

    for (auto round = 0; round < 32; ++round) {
        printf("round: %d\n", round);
        byte source_buffer[8];
        byte source_key[8];

        //Rand buffer
        printf("\tb : ");
        for (auto& _b : source_buffer) {
            _b = rand.next_range(1U, 255U);
            printf("%X ", int(_b));
        }
        printf("\n");
        
        //Rand key
        printf("\tk : ");
        for (auto& _b : source_key) {
            _b = rand.next_range(1U, 255U);
            printf("%X ", int(_b));
        }
        printf("\n");

        byte output_1[8];
        byte output_2[8];

        memset(output_1, 0, 8);
        memset(output_2, 0, 8);

        skl::skl_siphash_8(source_buffer, source_key, output_1);
        skl::skl_siphash_8(source_buffer, source_key, output_2);

        printf("\to1: ");
        for (auto _b : output_1) {
            printf("%X ", int(_b));
        }
        printf("\n");

        printf("\to2: ");
        for (auto _b : output_2) {
            printf("%X ", int(_b));
        }
        printf("\n");

        for (auto i = 0; i < 8; ++i) {
            ASSERT_EQ(output_1[i], output_2[i]);
        }

        for (auto& _b : source_key) {
            _b = rand.next_range(1U, 255U);
            printf("%X ", int(_b));
        }
        skl::skl_siphash_8(source_buffer, source_key, output_2);

        i32 eq_count = 0;
        for (auto i = 0; i < 8; ++i) {
            eq_count += i32(output_1[i] == output_2[i]);
        }
        ASSERT_TRUE(8 / 2 > eq_count);

        puts("");
    }
}

TEST(SKLCoreHashTests, siphash_4) {
    skl::SklRand rand{};

    for (auto round = 0; round < 32; ++round) {
        printf("round: %d\n", round);
        byte source_buffer[4];
        byte source_key[8];

        //Rand buffer
        printf("\tb : ");
        for (auto& _b : source_buffer) {
            _b = rand.next_range(1U, 255U);
            printf("%X ", int(_b));
        }
        printf("\n");
        
        //Rand key
        printf("\tk : ");
        for (auto& _b : source_key) {
            _b = rand.next_range(1U, 255U);
            printf("%X ", int(_b));
        }
        printf("\n");

        byte output_1[4];
        byte output_2[4];

        memset(output_1, 0, 4);
        memset(output_2, 0, 4);

        skl::skl_siphash_4(source_buffer, source_key, output_1);
        skl::skl_siphash_4(source_buffer, source_key, output_2);

        printf("\to1: ");
        for (auto _b : output_1) {
            printf("%X ", int(_b));
        }
        printf("\n");

        printf("\to2: ");
        for (auto _b : output_2) {
            printf("%X ", int(_b));
        }
        printf("\n");

        for (auto i = 0; i < 4; ++i) {
            ASSERT_EQ(output_1[i], output_2[i]);
        }

        for (auto& _b : source_key) {
            _b = rand.next_range(1U, 255U);
            printf("%X ", int(_b));
        }
        skl::skl_siphash_4(source_buffer, source_key, output_2);

        i32 eq_count = 0;
        for (auto i = 0; i < 4; ++i) {
            eq_count += i32(output_1[i] == output_2[i]);
        }
        ASSERT_TRUE(4 / 2 > eq_count);

        puts("");
    }
}