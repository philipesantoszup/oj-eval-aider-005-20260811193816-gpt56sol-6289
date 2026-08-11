#ifndef QOI_FORMAT_CODEC_QOI_H_
#define QOI_FORMAT_CODEC_QOI_H_

#include "utils.h"

constexpr uint8_t QOI_OP_INDEX_TAG = 0x00;
constexpr uint8_t QOI_OP_DIFF_TAG  = 0x40;
constexpr uint8_t QOI_OP_LUMA_TAG  = 0x80;
constexpr uint8_t QOI_OP_RUN_TAG   = 0xc0;
constexpr uint8_t QOI_OP_RGB_TAG   = 0xfe;
constexpr uint8_t QOI_OP_RGBA_TAG  = 0xff;
constexpr uint8_t QOI_PADDING[8] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u};
constexpr uint8_t QOI_MASK_2 = 0xc0;

/**
 * @brief Encode raw pixel data from standard input in QOI format.
 *
 * @param[in] width image width in pixels
 * @param[in] height image height in pixels
 * @param[in] channels number of color channels, 3 = RGB, 4 = RGBA
 * @param[in] colorspace image color space, 0 = sRGB with linear alpha,
 *                        1 = all channels linear
 *
 * @return true on success, false for invalid parameters or an I/O failure
 */
inline bool QoiEncode(
    uint32_t width,
    uint32_t height,
    uint8_t channels,
    uint8_t colorspace = 0
);

/**
 * @brief Decode QOI data from standard input to raw pixel data.
 *
 * @param[out] width image width in pixels
 * @param[out] height image height in pixels
 * @param[out] channels number of color channels, 3 = RGB, 4 = RGBA
 * @param[out] colorspace image color space, 0 = sRGB with linear alpha,
 *                         1 = all channels linear
 *
 * @return true if the input is a valid QOI stream, false otherwise
 */
inline bool QoiDecode(
    uint32_t &width,
    uint32_t &height,
    uint8_t &channels,
    uint8_t &colorspace
);

inline bool QoiEncode(
    uint32_t width,
    uint32_t height,
    uint8_t channels,
    uint8_t colorspace
) {
    if (width == 0u || height == 0u) {
        return false;
    }
    if (channels != 3u && channels != 4u) {
        return false;
    }
    if (colorspace > 1u) {
        return false;
    }

    QoiWriteChar('q');
    QoiWriteChar('o');
    QoiWriteChar('i');
    QoiWriteChar('f');
    QoiWriteU32(width);
    QoiWriteU32(height);
    QoiWriteU8(channels);
    QoiWriteU8(colorspace);

    if (!std::cout) {
        return false;
    }

    uint8_t history[64][4];
    std::memset(history, 0, sizeof(history));

    uint8_t previous_r = 0u;
    uint8_t previous_g = 0u;
    uint8_t previous_b = 0u;
    uint8_t previous_a = 255u;

    unsigned int run = 0u;
    const uint64_t pixel_count =
        static_cast<uint64_t>(width) * static_cast<uint64_t>(height);

    const auto wrap_to_signed_byte = [](int value) {
        if (value < -128) {
            value += 256;
        } else if (value > 127) {
            value -= 256;
        }
        return value;
    };

    for (uint64_t i = 0; i < pixel_count; ++i) {
        const uint8_t r = QoiReadU8();
        if (!std::cin) {
            return false;
        }

        const uint8_t g = QoiReadU8();
        if (!std::cin) {
            return false;
        }

        const uint8_t b = QoiReadU8();
        if (!std::cin) {
            return false;
        }

        uint8_t a = 255u;
        if (channels == 4u) {
            a = QoiReadU8();
            if (!std::cin) {
                return false;
            }
        }

        const bool same_as_previous =
            r == previous_r &&
            g == previous_g &&
            b == previous_b &&
            a == previous_a;

        if (same_as_previous) {
            ++run;

            if (run == 62u || i + 1u == pixel_count) {
                QoiWriteU8(static_cast<uint8_t>(
                    QOI_OP_RUN_TAG | static_cast<uint8_t>(run - 1u)
                ));
                run = 0u;
            }
        } else {
            if (run > 0u) {
                QoiWriteU8(static_cast<uint8_t>(
                    QOI_OP_RUN_TAG | static_cast<uint8_t>(run - 1u)
                ));
                run = 0u;
            }

            const int index = QoiColorHash(r, g, b, a);
            const bool found_in_history =
                history[index][0] == r &&
                history[index][1] == g &&
                history[index][2] == b &&
                history[index][3] == a;

            if (found_in_history) {
                QoiWriteU8(static_cast<uint8_t>(
                    QOI_OP_INDEX_TAG | static_cast<uint8_t>(index)
                ));
            } else {
                history[index][0] = r;
                history[index][1] = g;
                history[index][2] = b;
                history[index][3] = a;

                if (a == previous_a) {
                    const int dr = wrap_to_signed_byte(
                        static_cast<int>(r) - static_cast<int>(previous_r)
                    );
                    const int dg = wrap_to_signed_byte(
                        static_cast<int>(g) - static_cast<int>(previous_g)
                    );
                    const int db = wrap_to_signed_byte(
                        static_cast<int>(b) - static_cast<int>(previous_b)
                    );

                    if (
                        dr >= -2 && dr <= 1 &&
                        dg >= -2 && dg <= 1 &&
                        db >= -2 && db <= 1
                    ) {
                        QoiWriteU8(static_cast<uint8_t>(
                            QOI_OP_DIFF_TAG |
                            static_cast<uint8_t>((dr + 2) << 4) |
                            static_cast<uint8_t>((dg + 2) << 2) |
                            static_cast<uint8_t>(db + 2)
                        ));
                    } else {
                        const int dr_dg = wrap_to_signed_byte(dr - dg);
                        const int db_dg = wrap_to_signed_byte(db - dg);

                        if (
                            dg >= -32 && dg <= 31 &&
                            dr_dg >= -8 && dr_dg <= 7 &&
                            db_dg >= -8 && db_dg <= 7
                        ) {
                            QoiWriteU8(static_cast<uint8_t>(
                                QOI_OP_LUMA_TAG |
                                static_cast<uint8_t>(dg + 32)
                            ));
                            QoiWriteU8(static_cast<uint8_t>(
                                static_cast<uint8_t>((dr_dg + 8) << 4) |
                                static_cast<uint8_t>(db_dg + 8)
                            ));
                        } else {
                            QoiWriteU8(QOI_OP_RGB_TAG);
                            QoiWriteU8(r);
                            QoiWriteU8(g);
                            QoiWriteU8(b);
                        }
                    }
                } else {
                    QoiWriteU8(QOI_OP_RGBA_TAG);
                    QoiWriteU8(r);
                    QoiWriteU8(g);
                    QoiWriteU8(b);
                    QoiWriteU8(a);
                }
            }
        }

        if (!std::cout) {
            return false;
        }

        previous_r = r;
        previous_g = g;
        previous_b = b;
        previous_a = a;
    }

    for (uint8_t padding_byte : QOI_PADDING) {
        QoiWriteU8(padding_byte);
    }

    return static_cast<bool>(std::cout);
}

inline bool QoiDecode(
    uint32_t &width,
    uint32_t &height,
    uint8_t &channels,
    uint8_t &colorspace
) {
    width = 0u;
    height = 0u;
    channels = 0u;
    colorspace = 0u;

    const std::ios::fmtflags original_flags = std::cin.flags();
    std::cin.unsetf(std::ios::skipws);

    const char c1 = QoiReadChar();
    if (!std::cin) {
        std::cin.flags(original_flags);
        return false;
    }

    const char c2 = QoiReadChar();
    if (!std::cin) {
        std::cin.flags(original_flags);
        return false;
    }

    const char c3 = QoiReadChar();
    if (!std::cin) {
        std::cin.flags(original_flags);
        return false;
    }

    const char c4 = QoiReadChar();
    const bool magic_was_read = static_cast<bool>(std::cin);
    std::cin.flags(original_flags);

    if (!magic_was_read) {
        return false;
    }
    if (c1 != 'q' || c2 != 'o' || c3 != 'i' || c4 != 'f') {
        return false;
    }

    width = QoiReadU32();
    if (!std::cin) {
        return false;
    }

    height = QoiReadU32();
    if (!std::cin) {
        return false;
    }

    channels = QoiReadU8();
    if (!std::cin) {
        return false;
    }

    colorspace = QoiReadU8();
    if (!std::cin) {
        return false;
    }

    if (width == 0u || height == 0u) {
        return false;
    }
    if (channels != 3u && channels != 4u) {
        return false;
    }
    if (colorspace > 1u) {
        return false;
    }

    const uint64_t pixel_count =
        static_cast<uint64_t>(width) * static_cast<uint64_t>(height);

    uint8_t history[64][4];
    std::memset(history, 0, sizeof(history));

    uint8_t r = 0u;
    uint8_t g = 0u;
    uint8_t b = 0u;
    uint8_t a = 255u;

    unsigned int run = 0u;

    for (uint64_t i = 0; i < pixel_count; ++i) {
        if (run > 0u) {
            --run;
        } else {
            const uint8_t first_byte = QoiReadU8();
            if (!std::cin) {
                return false;
            }

            if (first_byte == QOI_OP_RGB_TAG) {
                r = QoiReadU8();
                if (!std::cin) {
                    return false;
                }

                g = QoiReadU8();
                if (!std::cin) {
                    return false;
                }

                b = QoiReadU8();
                if (!std::cin) {
                    return false;
                }
            } else if (first_byte == QOI_OP_RGBA_TAG) {
                r = QoiReadU8();
                if (!std::cin) {
                    return false;
                }

                g = QoiReadU8();
                if (!std::cin) {
                    return false;
                }

                b = QoiReadU8();
                if (!std::cin) {
                    return false;
                }

                a = QoiReadU8();
                if (!std::cin) {
                    return false;
                }
            } else {
                switch (first_byte & QOI_MASK_2) {
                    case QOI_OP_INDEX_TAG: {
                        const int index = first_byte & 0x3fu;
                        r = history[index][0];
                        g = history[index][1];
                        b = history[index][2];
                        a = history[index][3];
                        break;
                    }

                    case QOI_OP_DIFF_TAG: {
                        const int dr =
                            static_cast<int>((first_byte >> 4) & 0x03u) - 2;
                        const int dg =
                            static_cast<int>((first_byte >> 2) & 0x03u) - 2;
                        const int db =
                            static_cast<int>(first_byte & 0x03u) - 2;

                        r = static_cast<uint8_t>(static_cast<int>(r) + dr);
                        g = static_cast<uint8_t>(static_cast<int>(g) + dg);
                        b = static_cast<uint8_t>(static_cast<int>(b) + db);
                        break;
                    }

                    case QOI_OP_LUMA_TAG: {
                        const uint8_t second_byte = QoiReadU8();
                        if (!std::cin) {
                            return false;
                        }

                        const int dg =
                            static_cast<int>(first_byte & 0x3fu) - 32;
                        const int dr_dg =
                            static_cast<int>((second_byte >> 4) & 0x0fu) - 8;
                        const int db_dg =
                            static_cast<int>(second_byte & 0x0fu) - 8;

                        r = static_cast<uint8_t>(
                            static_cast<int>(r) + dg + dr_dg
                        );
                        g = static_cast<uint8_t>(
                            static_cast<int>(g) + dg
                        );
                        b = static_cast<uint8_t>(
                            static_cast<int>(b) + dg + db_dg
                        );
                        break;
                    }

                    case QOI_OP_RUN_TAG: {
                        const unsigned int encoded_run =
                            static_cast<unsigned int>(first_byte & 0x3fu) + 1u;
                        const uint64_t remaining_pixels = pixel_count - i;

                        if (
                            static_cast<uint64_t>(encoded_run) >
                            remaining_pixels
                        ) {
                            return false;
                        }

                        run = encoded_run - 1u;
                        break;
                    }

                    default:
                        return false;
                }
            }

            const int index = QoiColorHash(r, g, b, a);
            history[index][0] = r;
            history[index][1] = g;
            history[index][2] = b;
            history[index][3] = a;
        }

        QoiWriteU8(r);
        QoiWriteU8(g);
        QoiWriteU8(b);
        if (channels == 4u) {
            QoiWriteU8(a);
        }

        if (!std::cout) {
            return false;
        }
    }

    bool valid_padding = true;
    for (uint8_t expected_byte : QOI_PADDING) {
        const uint8_t actual_byte = QoiReadU8();
        if (!std::cin) {
            return false;
        }
        if (actual_byte != expected_byte) {
            valid_padding = false;
        }
    }

    return valid_padding && static_cast<bool>(std::cout);
}

#endif // QOI_FORMAT_CODEC_QOI_H_
