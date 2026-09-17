#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace esphome
{
    namespace philips_coffee_machine
    {
        /// @brief Computes the CRC-16/CCITT checksum used by the Philips coffee machine protocol.
        /// Polynomial 0x1021, init 0xAAAA, 8 bit input fed MSB first, not reflected, no final XOR.
        inline uint16_t crc16(const uint8_t *data, size_t length)
        {
            uint16_t crc = 0xAAAA;
            for (size_t i = 0; i < length; i++)
            {
                crc ^= static_cast<uint16_t>(data[i]) << 8;
                for (uint8_t bit = 0; bit < 8; bit++)
                    crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021) : static_cast<uint16_t>(crc << 1);
            }
            return crc;
        }

        /// @brief Appends the two 6-bit checksum bytes for `content` and returns the full message.
        inline std::vector<uint8_t> pack_message(std::vector<uint8_t> content)
        {
            uint16_t crc = crc16(content.data(), content.size());
            content.push_back(static_cast<uint8_t>((crc & 0xFF) >> 2));
            content.push_back(static_cast<uint8_t>((crc >> 8) >> 2));
            return content;
        }

        /// @brief Verifies that the trailing two bytes of `data` match the checksum of the bytes preceding them.
        inline bool verify_checksum(const uint8_t *data, size_t length)
        {
            if (length < 2)
                return false;
            uint16_t crc = crc16(data, length - 2);
            return data[length - 2] == static_cast<uint8_t>((crc & 0xFF) >> 2) &&
                   data[length - 1] == static_cast<uint8_t>((crc >> 8) >> 2);
        }

    } // namespace philips_coffee_machine
} // namespace esphome
