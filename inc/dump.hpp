// Copyright (c) 2016 Steinwurf ApS
// All Rights Reserved
//
// Distributed under the "BSD License". See the accompanying LICENSE.rst file.

#define NOMINMAX
#pragma once
#include <cassert>
#include <cstdint>
#include <algorithm>
#include <ostream>
#include <stdint.h>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>

namespace hex
{
/// @brief Small class which wraps a pointer and a size and
///        provides an output stream operator to print the content
///        as a hexdump.
///
/// The hexdump format consist of three columns per row of
/// bytes. The first column is the byte offset of the first byte
/// in the row, the second column is the byte values in hex of
/// those bytes (16 bytes per row) and the third column is the
/// ascii representation of those bytes.
///
/// Example of two rows (we removed the middle 6 bytes due to space
/// limitations):
///
/// @code
/// 0000  be 40 04 71 45 XXX cd 90 e5 51 31  .@.qE XXX ...Q1
/// 0010  9d 41 4f 37 05 XXX a9 d5 1e c7 93  .AO7. XXX .....
/// @endcode
struct dump
{
    /// @param data The pointer to the data which we want to dump
    /// @param max_size The maximum size in bytes of the data which we want
    ///        to dump.
    dump(const uint8_t* data, uint32_t max_size) :
        m_data(data),
        m_max_size(max_size),
        m_size(max_size)
    {
        assert(m_data);
        assert(m_max_size);
        assert(m_size);
    }

    /// @param vector The vector instance which we want to dump
    template<class PodType, class Allocator>
    dump(const std::vector<PodType, Allocator>& vector)
    {
        m_size = uint32_t(vector.size() * sizeof(PodType));
        m_max_size = m_size;
        m_data = reinterpret_cast<const uint8_t*>(&vector[0]);

        assert(m_data);
        assert(m_max_size);
        assert(m_size);
    }

    /// @param size The size in bytes we wish to dump.
    /// The actual number of bytes dumped will equal the smallest value of
    /// either size or the max_size which was given as a parameter to
    /// the this object's constructor.
    void set_size(uint32_t size)
    {
        assert(m_size > 0);
        m_size = size;
    }

    /// The pointer to the data that should be printed
    const uint8_t* m_data;

    /// The maximum number of bytes that can be printed
    uint32_t m_max_size;

    /// The number of bytes that the user wants to print
    uint32_t m_size;
};

/// Helper function to convert a hexadecimal string to an equivalent byte
/// vector. For example, the string "ff 02 03 04" is converted to a
/// std::vector<uint8_t> containing { 0xff, 0x02, 0x03, 0x04 }.
///
/// @param hex_string The input string of hexadecimal characters
/// @return The byte vector created from the string representation
inline std::vector<uint8_t> parse_hex_string(const std::string& hex_string)
{
    std::vector<uint8_t> result;
    std::istringstream in(hex_string);
    in >> std::hex;

    uint32_t value;
    while (in >> value)
    {
        assert(value <= 255U && "Invalid hex byte in string");
        result.push_back(static_cast<uint8_t>(value));
    }

    return result;
}

/// Compares two hex::dump objects to see whether they are
/// equal, i.e. they contain exactly the same data.
///
/// @param a The first hex::dump object
/// @param b The second hex::dump object
/// @return True if the hex::dump objects contain the same data,
///         otherwise false.
inline bool operator==(const dump& a, const dump& b)
{
    if (a.m_size != b.m_size)
    {
        return false;
    }

    // They have the same size - do they point to the same data?
    if (a.m_data == b.m_data)
    {
        return true;
    }

    // It is two different buffers - is the content equal?
    return std::equal(a.m_data, a.m_data + a.m_size, b.m_data);
}

/// The actual output operator which prints the data buffer to
/// the choosen output stream.
///
/// @param out The output stream
///
/// @param hex The hexdump struct initialized with the data and size
///        that should be printed
///
/// @return the used output stream
inline std::ostream& operator<<(std::ostream& out, const dump& hex)
{
    const uint8_t* data = hex.m_data;
    uint32_t size = std::min(hex.m_size, hex.m_max_size);

    assert(data);
    assert(size > 0);

    // don't change formatting for out
    std::ostream s(out.rdbuf());
    s << std::hex << std::setfill('0');

    std::string buf;
    buf.reserve(17); // premature optimization

    for (uint32_t i = 0; i < size; ++i)
    {
        if ((i % 16) == 0)
        {
            if (i)
            {
                s << "  " << buf << std::endl;
                buf.clear();
            }
            s << std::setw(4) << i << ' ';
        }

        uint8_t c = data[i];

        s << ' ' << std::setw(2) << (uint32_t) c;
        buf += (0x20 <= c && c <= 0x7e) ? c : '.';
    }

    if (size % 16)
    {
        // If size if not a multiple of 16 there will be some empty
        // columns in our print out. The remainder i.e. 16 - (size %
        // 16) e.g. for some basic cases:
        //
        // size = 15
        // remainder = 16 - (15 % 16) = 1
        //
        // size = 17
        // remainder = 16 - (17 % 16) = 15
        //
        // and so forth.
        uint32_t remainder = 16 - (size % 16);

        // We add 3 space for each missing character in the output
        s << std::string(3 * remainder, ' ');
    }

    s << "  " << buf << std::endl;

    if (size < hex.m_max_size && 16 < hex.m_max_size)
    {
        uint32_t last_row = (hex.m_max_size / 16) * 16;

        s << "...." << std::endl;
        s << std::setw(4) << last_row << std::endl;
    }

    return out;
}
}
