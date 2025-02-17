#include <gtest/gtest.h>

#include <skl_stream>

TEST(stream_ut, stream_basics) {
    byte                 buffer[1024U];
    skl::skl_buffer_view buffer_view{std::size(buffer), buffer};
    auto&                stream = skl::skl_stream::make(buffer_view);

    ASSERT_EQ(stream.length(), std::size(buffer));
    ASSERT_EQ(stream.remaining(), std::size(buffer));
    ASSERT_EQ(stream.position(), 0U);
    ASSERT_FALSE(stream.eos());
    ASSERT_TRUE(stream.is_valid());
    ASSERT_EQ(stream.front(), buffer);
}

TEST(stream_ut, stream_basics_2) {
    {
        skl::skl_buffer_view buffer_view{};
        auto&                stream = skl::skl_stream::make(buffer_view);
        ASSERT_FALSE(stream.is_valid());
    }

    {
        byte                 buffer[1024U];
        skl::skl_buffer_view buffer_view{0U, buffer};
        auto&                stream = skl::skl_stream::make(buffer_view);
        ASSERT_FALSE(stream.is_valid());
    }
}

TEST(stream_ut, stream_seek) {
    byte                 buffer[1024U];
    skl::skl_buffer_view buffer_view{std::size(buffer), buffer};
    auto&                stream = skl::skl_stream::make(buffer_view);

    stream.seek_forward(stream.length());
    ASSERT_EQ(stream.length(), std::size(buffer));
    ASSERT_EQ(stream.remaining(), 0U);
    ASSERT_EQ(stream.position(), stream.length());
    ASSERT_TRUE(stream.eos());
    ASSERT_TRUE(stream.is_valid());
    ASSERT_EQ(stream.front(), buffer + std::size(buffer));

    stream.seek_backward(stream.length());
    ASSERT_EQ(stream.length(), std::size(buffer));
    ASSERT_EQ(stream.remaining(), std::size(buffer));
    ASSERT_EQ(stream.position(), 0U);
    ASSERT_FALSE(stream.eos());
    ASSERT_TRUE(stream.is_valid());
    ASSERT_EQ(stream.front(), buffer);
}

TEST(stream_ut, stream_seek_2) {
    byte                 buffer[1024U];
    skl::skl_buffer_view buffer_view{std::size(buffer), buffer};
    auto&                stream = skl::skl_stream::make(buffer_view);

    stream.seek_end();
    ASSERT_EQ(stream.length(), std::size(buffer));
    ASSERT_EQ(stream.remaining(), 0U);
    ASSERT_EQ(stream.position(), stream.length());
    ASSERT_TRUE(stream.eos());
    ASSERT_TRUE(stream.is_valid());
    ASSERT_EQ(stream.front(), buffer + std::size(buffer));

    stream.seek_start();
    ASSERT_EQ(stream.length(), std::size(buffer));
    ASSERT_EQ(stream.remaining(), std::size(buffer));
    ASSERT_EQ(stream.position(), 0U);
    ASSERT_FALSE(stream.eos());
    ASSERT_TRUE(stream.is_valid());
    ASSERT_EQ(stream.front(), buffer);

    stream.seek_exact(512U);
    ASSERT_EQ(stream.length(), std::size(buffer));
    ASSERT_EQ(stream.remaining(), 512U);
    ASSERT_EQ(stream.position(), 512U);
    ASSERT_FALSE(stream.eos());
    ASSERT_TRUE(stream.is_valid());
    ASSERT_EQ(stream.front(), buffer + 512U);

    stream.reset();
    ASSERT_EQ(stream.length(), std::size(buffer));
    ASSERT_EQ(stream.remaining(), std::size(buffer));
    ASSERT_EQ(stream.position(), 0U);
    ASSERT_FALSE(stream.eos());
    ASSERT_TRUE(stream.is_valid());
    ASSERT_EQ(stream.front(), buffer);
}

TEST(stream_ut, stream_zero) {
    byte                 buffer[1024U];
    skl::skl_buffer_view buffer_view{std::size(buffer), buffer};
    auto&                stream = skl::skl_stream::make(buffer_view);

    for (auto& byte : buffer) {
        byte = 0xf0U;
    }

    for (u32 i = 0U; i < stream.length(); ++i) {
        ASSERT_EQ(stream[i], 0xf0U);
    }

    stream.zero();

    for (u32 i = 0U; i < stream.length(); ++i) {
        ASSERT_EQ(stream[i], 0x00U);
    }
}

TEST(stream_ut, stream_zero_remaining) {
    byte                 buffer[1024U];
    skl::skl_buffer_view buffer_view{std::size(buffer), buffer};
    auto&                stream = skl::skl_stream::make(buffer_view);

    for (auto& byte : buffer) {
        byte = 0xf0U;
    }

    for (u32 i = 0U; i < stream.length(); ++i) {
        ASSERT_EQ(stream[i], 0xf0U);
    }

    stream.seek_forward(512);
    stream.zero_remaining();

    for (u32 i = 512U; i < stream.length(); ++i) {
        ASSERT_EQ(stream[i], 0x00U);
    }

    for (u32 i = 0U; i < 512U; ++i) {
        ASSERT_EQ(stream[i], 0xf0U);
    }
}

TEST(stream_ut, stream_fits) {
    byte                 buffer[1024U];
    skl::skl_buffer_view buffer_view{std::size(buffer), buffer};
    auto&                stream = skl::skl_stream::make(buffer_view);

    ASSERT_TRUE(stream.fits(std::size(buffer)));
    stream.seek_forward(1);
    ASSERT_FALSE(stream.fits(std::size(buffer)));
    ASSERT_TRUE(stream.fits(std::size(buffer) - 1U));
    stream.seek_end();
    ASSERT_FALSE(stream.fits(1U));
    ASSERT_TRUE(stream.fits(0U));
    stream.seek_start();
    ASSERT_TRUE(stream.fits(std::size(buffer)));
}

TEST(stream_ut, stream_write) {
    byte                 buffer[1024U];
    skl::skl_buffer_view buffer_view{std::size(buffer), buffer};
    auto&                stream = skl::skl_stream::make(buffer_view);

    byte write_buffer[1024U];
    for (auto& byte : write_buffer) {
        byte = 0xfeU;
    }

    auto b_result = stream.write(write_buffer);
    ASSERT_TRUE(b_result);
    ASSERT_TRUE(stream.is_valid());
    ASSERT_TRUE(stream.eos());
    ASSERT_EQ(stream.position(), std::size(write_buffer));
    ASSERT_EQ(stream.position(), stream.length());
    ASSERT_EQ(stream.remaining(), 0U);

    for (u32 i = 0U; i < stream.length(); ++i) {
        ASSERT_EQ(stream[i], 0xfeU);
    }

    b_result = stream.write(write_buffer);
    ASSERT_FALSE(b_result);
    ASSERT_TRUE(stream.is_valid());
    ASSERT_TRUE(stream.eos());
    ASSERT_EQ(stream.position(), std::size(write_buffer));
    ASSERT_EQ(stream.position(), stream.length());
    ASSERT_EQ(stream.remaining(), 0U);

    for (u32 i = 0U; i < stream.length(); ++i) {
        ASSERT_EQ(stream[i], 0xfeU);
    }
}

TEST(stream_ut, stream_write_2) {
    byte                 buffer[1024U];
    skl::skl_buffer_view buffer_view{std::size(buffer), buffer};
    auto&                stream = skl::skl_stream::make(buffer_view);

    stream.zero();

    byte write_buffer[512U];
    for (auto& byte : write_buffer) {
        byte = 0xfeU;
    }

    auto b_result = stream.write(write_buffer);
    ASSERT_TRUE(b_result);
    ASSERT_TRUE(stream.is_valid());
    ASSERT_FALSE(stream.eos());
    ASSERT_EQ(stream.position(), std::size(write_buffer));
    ASSERT_NE(stream.position(), stream.length());
    ASSERT_EQ(stream.remaining(), 512U);

    for (u32 i = 0U; i < stream.length(); ++i) {
        if (i < 512U) {
            ASSERT_EQ(stream[i], 0xfeU);
        } else {
            ASSERT_EQ(stream[i], 0x00U);
        }
    }

    b_result = stream.write(write_buffer);
    ASSERT_TRUE(b_result);
    ASSERT_TRUE(stream.is_valid());
    ASSERT_TRUE(stream.eos());
    ASSERT_EQ(stream.position(), std::size(buffer));
    ASSERT_EQ(stream.position(), stream.length());
    ASSERT_EQ(stream.remaining(), 0U);

    for (u32 i = 0U; i < stream.length(); ++i) {
        ASSERT_EQ(stream[i], 0xfeU);
    }
}

TEST(stream_ut, stream_write_4) {
    byte                 buffer[8U];
    skl::skl_buffer_view buffer_view{std::size(buffer), buffer};
    auto&                stream = skl::skl_stream::make(buffer_view);

    stream.zero();

    const u8 a = 1U;
    stream.write(a);
    ASSERT_EQ(stream.position(), sizeof(a));
    ASSERT_EQ(stream.remaining(), stream.length() - sizeof(a));

    const u16 b = 5U;
    stream.write(b);
    ASSERT_EQ(stream.position(), sizeof(b) + sizeof(a));
    ASSERT_EQ(stream.remaining(), stream.length() - sizeof(b) - sizeof(a));

    ASSERT_EQ(stream.position(), 3U);
    ASSERT_EQ(stream.remaining(), 5U);

    const u32 c = 0xFFFFFFFFU;
    stream.write(c);

    ASSERT_EQ(stream.position(), 7U);
    ASSERT_EQ(stream.remaining(), 1U);

    ASSERT_FALSE(stream.write_safe(c));
    ASSERT_EQ(0xFFU, stream[stream.length() - 2U]);
    ASSERT_EQ(0U, stream[stream.length() - 1U]);
}

TEST(stream_ut, stream_write_unsafe) {
    byte                 buffer[1024U];
    skl::skl_buffer_view buffer_view{std::size(buffer), buffer};
    auto&                stream = skl::skl_stream::make(buffer_view);

    byte write_buffer[1024U];
    for (auto& byte : write_buffer) {
        byte = 0xfeU;
    }

    stream.write_unsafe(write_buffer);
    ASSERT_TRUE(stream.is_valid());
    ASSERT_TRUE(stream.eos());
    ASSERT_EQ(stream.position(), std::size(write_buffer));
    ASSERT_EQ(stream.position(), stream.length());
    ASSERT_EQ(stream.remaining(), 0U);

    for (u32 i = 0U; i < stream.length(); ++i) {
        ASSERT_EQ(stream[i], 0xfeU);
    }

    ASSERT_FALSE(stream.write(write_buffer));
    ASSERT_TRUE(stream.is_valid());
    ASSERT_TRUE(stream.eos());
    ASSERT_EQ(stream.position(), std::size(write_buffer));
    ASSERT_EQ(stream.position(), stream.length());
    ASSERT_EQ(stream.remaining(), 0U);

    for (u32 i = 0U; i < stream.length(); ++i) {
        ASSERT_EQ(stream[i], 0xfeU);
    }
}

TEST(stream_ut, stream_write_str) {
    {
        byte                 buffer[24U];
        skl::skl_buffer_view buffer_view{std::size(buffer), buffer};
        auto&                stream = skl::skl_stream::make(buffer_view);

        stream.zero();

        for(auto& _b : buffer){
            _b = 55U;
        }

        ASSERT_TRUE(stream.write_str("MY_STR"));
        ASSERT_EQ(stream.position(), 7U);
        ASSERT_EQ(stream[0U], 'M');
        ASSERT_EQ(stream[5U], 'R');
        ASSERT_EQ(stream[6U], 0U);
    }

    {
        byte                 buffer[24U];
        skl::skl_buffer_view buffer_view{std::size(buffer), buffer};
        auto&                stream = skl::skl_stream::make(buffer_view);

        stream.zero();

        ASSERT_FALSE(stream.write_str("MY_LONG_LONG_LONG_LONG_STR"));
        ASSERT_EQ(stream.position(), 0U);
    }

    {
        byte                 buffer[24U];
        skl::skl_buffer_view buffer_view{std::size(buffer), buffer};
        auto&                stream = skl::skl_stream::make(buffer_view);

        stream.zero();

        ASSERT_TRUE(stream.write_str("MY_STR"));
        ASSERT_EQ(stream.position(), 7U);
        ASSERT_EQ(stream[0U], 'M');
        ASSERT_EQ(stream[5U], 'R');
        ASSERT_EQ(stream[6U], 0U);

        ASSERT_FALSE(stream.write_str("MY_LONG_LONG_LONG_LONG_STR"));
        ASSERT_EQ(stream.position(), 7U);
        ASSERT_EQ(stream[0U], 'M');
        ASSERT_EQ(stream[5U], 'R');
        ASSERT_EQ(stream[6U], 0U);

        ASSERT_TRUE(stream.write_str("MY_STR2"));
        ASSERT_EQ(stream.position(), 15U);
        ASSERT_EQ(stream[0U], 'M');
        ASSERT_EQ(stream[5U], 'R');
        ASSERT_EQ(stream[6U], 0U);
        ASSERT_EQ(stream[7U], 'M');
        ASSERT_EQ(stream[12U], 'R');
        ASSERT_EQ(stream[13U], '2');
        ASSERT_EQ(stream[14U], 0U);
    }
}

TEST(stream_ut, stream_write_str_2) {
    {
        byte                 buffer[24U];
        skl::skl_buffer_view buffer_view{std::size(buffer), buffer};
        auto&                stream = skl::skl_stream::make(buffer_view);

        stream.zero();

        constexpr auto* my_str      = "MY_STR";
        constexpr auto* my_long_str = "MY_LONG_LONG_LONG_LONG_STR";

        ASSERT_TRUE(stream.write_str(my_str, std::size("MY_STR")));
        ASSERT_EQ(stream.position(), 7U);
        ASSERT_EQ(stream[0U], 'M');
        ASSERT_EQ(stream[5U], 'R');
        ASSERT_EQ(stream[6U], 0U);

        ASSERT_TRUE(stream.write_str(my_str, std::size("MY_STR")));
        ASSERT_EQ(stream.position(), 14U);
        ASSERT_EQ(stream[7U], 'M');
        ASSERT_EQ(stream[12U], 'R');
        ASSERT_EQ(stream[13U], 0U);

        ASSERT_FALSE(stream.write_str(my_long_str, std::size("MY_LONG_LONG_LONG_LONG_STR")));
        ASSERT_EQ(stream.position(), 14U);
        ASSERT_EQ(stream[7U], 'M');
        ASSERT_EQ(stream[12U], 'R');
        ASSERT_EQ(stream[13U], 0U);
    }

    {
        byte                 buffer[27U];
        skl::skl_buffer_view buffer_view{std::size(buffer), buffer};
        auto&                stream = skl::skl_stream::make(buffer_view);

        stream.zero();

        constexpr auto* my_long_str = "MY_LONG_LONG_LONG_LONG_STR";

        ASSERT_TRUE(stream.write_str(my_long_str, std::size("MY_LONG_LONG_LONG_LONG_STR")));
        ASSERT_EQ(stream.position(), 27U);
        ASSERT_EQ(stream[0U], 'M');
        ASSERT_EQ(stream[25U], 'R');
        ASSERT_EQ(stream[26U], 0U);
    }
}

TEST(stream_ut, stream_write_str_3) {
    {
        byte                 buffer[27U];
        skl::skl_buffer_view buffer_view{std::size(buffer), buffer};
        auto&                stream = skl::skl_stream::make(buffer_view);

        stream.zero();

        char bad_string[] = "MY_LONG_LONG_LONG_LONG_STR";

        //Remove nulltermination
        bad_string[26] = 'C';

        ASSERT_FALSE(stream.write_str(bad_string, std::size(bad_string)));

        ASSERT_EQ(stream.position(), 0U);
        ASSERT_EQ(stream.remaining(), 27U);

        //Add nulltermination
        bad_string[26] = 0U;

        ASSERT_TRUE(stream.write_str(bad_string, std::size(bad_string)));

        ASSERT_EQ(stream[0U], 'M');
        ASSERT_EQ(stream[25U], 'R');
        ASSERT_EQ(stream[26U], 0);
    }

    {
        byte                 buffer[27U];
        skl::skl_buffer_view buffer_view{std::size(buffer), buffer};
        auto&                stream = skl::skl_stream::make(buffer_view);

        stream.zero();

        char bad_string[] = "MY_LONG_LONG_LONG_LONG_STR";

        //Remove nulltermination
        bad_string[26] = 'C';

        ASSERT_FALSE(stream.write_str(bad_string));

        ASSERT_EQ(stream.position(), 0U);
        ASSERT_EQ(stream.remaining(), 27U);

        //Add nulltermination
        bad_string[26] = 0U;

        ASSERT_TRUE(stream.write_str(bad_string));

        ASSERT_EQ(stream[0U], 'M');
        ASSERT_EQ(stream[25U], 'R');
        ASSERT_EQ(stream[26U], 0);
    }
}

TEST(stream_ut, stream_write_str_unsafe) {
    {
        byte                 buffer[24U];
        skl::skl_buffer_view buffer_view{std::size(buffer), buffer};
        auto&                stream = skl::skl_stream::make(buffer_view);

        stream.zero();

        stream.write_str_unsafe("MY_STR");
        ASSERT_EQ(stream.position(), 7U);
        ASSERT_EQ(stream[0U], 'M');
        ASSERT_EQ(stream[5U], 'R');
        ASSERT_EQ(stream[6U], 0U);
    }

    {
        byte                 buffer[24U];
        skl::skl_buffer_view buffer_view{std::size(buffer), buffer};
        auto&                stream = skl::skl_stream::make(buffer_view);
        ASSERT_DEATH(stream.write_str_unsafe("MY_LONG_LONG_LONG_LONG_STR"), ".*");
    }
}

TEST(stream_ut, stream_read) {
    byte                 buffer[27U];
    skl::skl_buffer_view buffer_view{std::size(buffer), buffer};
    auto&                stream = skl::skl_stream::make(buffer_view);

    stream.zero();

    const char long_string[] = "MY_LONG_LONG_LONG_LONG_STR";
    ASSERT_TRUE(stream.write_str(long_string));
    stream.reset();

    byte out_buffer[27U];
    ASSERT_TRUE(stream.read(out_buffer, 27U));
    ASSERT_EQ(0, strcmp(reinterpret_cast<const char*>(out_buffer), long_string));

    ASSERT_EQ(0U, stream.remaining());
    ASSERT_EQ(27U, stream.position());
    ASSERT_TRUE(stream.eos());
}

TEST(stream_ut, stream_read_2) {
    byte                 buffer[8U];
    skl::skl_buffer_view buffer_view{std::size(buffer), buffer};
    auto&                stream = skl::skl_stream::make(buffer_view);

    stream.zero();
    stream.write<u32>(2U);
    stream.write<u32>(4U);
    stream.reset();

    ASSERT_EQ(2U, stream.read<u32>());
    ASSERT_EQ(4U, stream.read<u32>());
    ASSERT_EQ(stream.try_read<u32>(32321U), 32321U);
}

#ifdef HAS_SKL_ASSERT
TEST(stream_ut, stream_seek_negative) {
    byte                 buffer[1024U];
    skl::skl_buffer_view buffer_view{std::size(buffer), buffer};
    auto&                stream = skl::skl_stream::make(buffer_view);

    ASSERT_DEATH([&stream]() {
        stream.seek_forward(stream.length() + 1U);
    }(), ".*");
}

TEST(stream_ut, stream_seek_negative_2) {
    byte                 buffer[1024U];
    skl::skl_buffer_view buffer_view{std::size(buffer), buffer};
    auto&                stream = skl::skl_stream::make(buffer_view);

    ASSERT_DEATH([&stream]() {
        stream.seek_backward(1U);
    }(), ".*");
}

TEST(stream_ut, stream_seek_negative_3) {
    byte                 buffer[1024U];
    skl::skl_buffer_view buffer_view{std::size(buffer), buffer};
    auto&                stream = skl::skl_stream::make(buffer_view);

    stream.seek_forward(stream.length());
    ASSERT_TRUE(stream.eos());

    ASSERT_DEATH([&stream]() {
        stream.seek_backward(stream.length() + 1U);
    }(), ".*");
}

TEST(stream_ut, stream_misc_negative) {
    byte                 buffer[1U];
    skl::skl_buffer_view buffer_view{std::size(buffer), buffer};
    auto&                stream = skl::skl_stream::make(buffer_view);

    ASSERT_DEATH([&]() {
        (void)stream.cast<u32>();
    }(), ".*");

    ASSERT_DEATH([&]() {
        (void)stream.cast_ref<u32>();
    }(), ".*");

    ASSERT_DEATH([&]() {
        (void)stream.cast_val<u32>();
    }(), ".*");

    ASSERT_DEATH([&]() {
        (void)stream.cast_buffer<u32>();
    }(), ".*");

    ASSERT_DEATH([&]() {
        (void)stream.cast_buffer_ref<u32>();
    }(), ".*");

    ASSERT_DEATH([&]() {
        (void)stream.cast_buffer_val<u32>();
    }(), ".*");

    ASSERT_DEATH([&]() {
        (void)stream.read<u32>();
    }(), ".*");

    ASSERT_DEATH([]() {
        skl::skl_buffer_view invalid_buffer_view{};
        auto&                invalid_stream = skl::skl_stream::make(invalid_buffer_view);
        (void)invalid_stream.count_non_zero();
    }(), ".*");

    ASSERT_DEATH([]() {
        skl::skl_buffer_view invalid_buffer_view{};
        auto&                invalid_stream = skl::skl_stream::make(invalid_buffer_view);
        (void)invalid_stream[0U];
    }(), ".*");

    ASSERT_DEATH([]() {
        skl::skl_buffer_view invalid_buffer_view{};
        auto&                invalid_stream = skl::skl_stream::make(invalid_buffer_view);
        (void)invalid_stream.skip_cstring();
    }(), ".*");

    ASSERT_DEATH([]() {
        skl::skl_buffer_view invalid_buffer_view{};
        auto&                invalid_stream = skl::skl_stream::make(invalid_buffer_view);
        const byte           one_byte{5U};
        (void)invalid_stream.write(&one_byte, 1U);
    }(), ".*");

    ASSERT_DEATH([]() {
        skl::skl_buffer_view invalid_buffer_view{};
        auto&                invalid_stream = skl::skl_stream::make(invalid_buffer_view);
        const byte           one_byte{5U};
        (void)invalid_stream.write_unsafe(&one_byte, 1U);
    }(), ".*");

    ASSERT_DEATH([]() {
        skl::skl_buffer_view invalid_buffer_view{};
        auto&                invalid_stream = skl::skl_stream::make(invalid_buffer_view);
        (void)invalid_stream.write_str("TEST");
    }(), ".*");

    ASSERT_DEATH([]() {
        skl::skl_buffer_view invalid_buffer_view{};
        auto&                invalid_stream = skl::skl_stream::make(invalid_buffer_view);
        (void)invalid_stream.write_str("TEST");
    }(), ".*");

    ASSERT_DEATH([]() {
        skl::skl_buffer_view invalid_buffer_view{};
        auto&                invalid_stream = skl::skl_stream::make(invalid_buffer_view);
        (void)invalid_stream.write_str("TEST", 5U);
    }(), ".*");

    ASSERT_DEATH([]() {
        skl::skl_buffer_view invalid_buffer_view{};
        auto&                invalid_stream = skl::skl_stream::make(invalid_buffer_view);
        (void)invalid_stream.write_str_unsafe("TEST");
    }(), ".*");

    ASSERT_DEATH([]() {
        skl::skl_buffer_view invalid_buffer_view{};
        auto&                invalid_stream = skl::skl_stream::make(invalid_buffer_view);
        (void)invalid_stream.zero();
    }(), ".*");

    ASSERT_DEATH([]() {
        skl::skl_buffer_view invalid_buffer_view{};
        auto&                invalid_stream = skl::skl_stream::make(invalid_buffer_view);
        (void)invalid_stream.zero_remaining();
    }(), ".*");

    ASSERT_DEATH([]() {
        skl::skl_buffer_view invalid_buffer_view{};
        auto&                invalid_stream = skl::skl_stream::make(invalid_buffer_view);
        (void)invalid_stream.read_from_file("");
    }(), ".*");

    ASSERT_DEATH([]() {
        skl::skl_buffer_view invalid_buffer_view{};
        auto&                invalid_stream = skl::skl_stream::make(invalid_buffer_view);
        (void)invalid_stream.read_from_text_file("");
    }(), ".*");

    ASSERT_DEATH([]() {
        skl::skl_buffer_view invalid_buffer_view{};
        auto&                invalid_stream = skl::skl_stream::make(invalid_buffer_view);
        byte                 one_byte{5U};
        (void)invalid_stream.read(&one_byte, 1U);
    }(), ".*");

    ASSERT_DEATH([]() {
        skl::skl_buffer_view invalid_buffer_view{};
        auto&                invalid_stream = skl::skl_stream::make(invalid_buffer_view);
        (void)invalid_stream.write_to_file("");
    }(), ".*");

    ASSERT_DEATH([]() {
        skl::skl_buffer_view invalid_buffer_view{};
        auto&                invalid_stream = skl::skl_stream::make(invalid_buffer_view);
        (void)invalid_stream.remaining_view();
    }(), ".*");

    ASSERT_DEATH([]() {
        skl::skl_buffer_view invalid_buffer_view{};
        auto&                invalid_stream = skl::skl_stream::make(invalid_buffer_view);
        (void)invalid_stream.sub_view(1U);
    }(), ".*");

    ASSERT_DEATH([&stream]() {
        (void)stream.sub_view(2U);
    }(), ".*");

    ASSERT_DEATH([&stream]() {
        (void)stream.sub_view(1U, 1U);
    }(), ".*");

    ASSERT_DEATH([]() {
        skl::skl_buffer_view invalid_buffer_view{};
        auto&                invalid_stream = skl::skl_stream::make(invalid_buffer_view);
        (void)invalid_stream.cstring_view();
    }(), ".*");

    ASSERT_FALSE(stream.fits(stream.remaining() + 1U));

    stream.seek_end();
    ASSERT_DEATH([&]() {
        (void)stream.remaining_view();
    }(), ".*");
    ASSERT_DEATH([&]() {
        (void)stream.c_str();
    }(), ".*");
    ASSERT_DEATH([&]() {
        (void)stream.cstring_view();
    }(), ".*");
}

TEST(stream_ut, stream_write_negative_1) {
    byte                 buffer[1U];
    skl::skl_buffer_view buffer_view{std::size(buffer), buffer};
    auto&                stream = skl::skl_stream::make(buffer_view);

    const u8 a = 1U;
    stream.write(a);
    ASSERT_EQ(stream.position(), 1U);
    ASSERT_EQ(stream.remaining(), 0U);

    ASSERT_DEATH([&]() {
        const u16 b = 5U;
        stream.write(b);
        ASSERT_EQ(stream.position(), sizeof(b) + sizeof(a));
        ASSERT_EQ(stream.remaining(), stream.length() - sizeof(b) - sizeof(a));
    }(), ".*");
}
#endif
