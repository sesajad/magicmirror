//
// Created by baloot on 22/07/24.
//

#ifndef PROTOCOLS_H
#define PROTOCOLS_H

#include <cstdint>
#include <numeric>
#include <span>
#include <boost/asio/ip/address_v4.hpp>


///@ brief Mockup ipv4_header for with no options.
//
//  IPv4 wire format:
//
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-------+-------+-------+-------+-------+-------+-------+------+   ---
// |version|header |    type of    |    total length in bytes     |    ^
// |  (4)  | length|    service    |                              |    |
// +-------+-------+-------+-------+-------+-------+-------+------+    |
// |        identification         |flags|    fragment offset     |    |
// +-------+-------+-------+-------+-------+-------+-------+------+  20 bytes
// | time to live  |   protocol    |       header checksum        |    |
// +-------+-------+-------+-------+-------+-------+-------+------+    |
// |                      source IPv4 address                     |    |
// +-------+-------+-------+-------+-------+-------+-------+------+    |
// |                   destination IPv4 address                   |    v
// +-------+-------+-------+-------+-------+-------+-------+------+   ---
// /                       options (if any)                       /
// +-------+-------+-------+-------+-------+-------+-------+------+

struct ipv4_header {
  constexpr static std::size_t size = 20;

  std::span<uint8_t, 20> data;

  explicit ipv4_header(const std::span<uint8_t, 20> &data) : data(data) {}

  inline void version(uint8_t value) { data[0] = (data[0] & 0x0F) | (value << 4); }
  inline void header_length(uint8_t value) { data[0] = (data[0] & 0xF0) | (value & 0x0F); }
  inline void type_of_service(uint8_t value) { data[1] = value; }
  inline void total_length(uint16_t value) { data[2] = value >> 8; data[3] = value & 0xFF; }
  inline void identification(uint16_t value) { data[4] = value >> 8; data[5] = value & 0xFF; }
  inline void dont_fragment(bool value) { data[6] = (data[6] & 0x3F) | (value << 6); }
  inline void more_fragments(bool value) { data[6] = (data[6] & 0xDF) | (value << 5); }
  inline void fragment_offset(uint16_t value) { data[6] = (data[6] & 0xE0) | ((value >> 8) & 0x1F); data[7] = value & 0xFF; }
  inline void time_to_live(uint8_t value) { data[8] = value; }
  inline void protocol(uint8_t value) { data[9] = value; }
  inline void header_checksum(uint16_t value) { data[10] = value >> 8; data[11] = value & 0xFF; }
  inline void source_address(uint32_t value) { data[12] = value >> 24; data[13] = (value >> 16) & 0xFF;
    data[14] = (value >> 8) & 0xFF; data[15] = value & 0xFF; }
  inline void destination_address(uint32_t value) { data[16] = value >> 24; data[17] = (value >> 16) & 0xFF;
    data[18] = (value >> 8) & 0xFF; data[19] = value & 0xFF; }

  inline void source_address(const boost::asio::ip::address_v4 &value) { source_address(value.to_ulong()); }
  inline void destination_address(const boost::asio::ip::address_v4 &value) { destination_address(value.to_ulong()); }

  void calculate_length(const std::span<uint8_t> body) {
    total_length(size + body.size());
  }

  void calculate_checksum(const std::span<uint8_t> body) {
    std::span<uint16_t, size / 2> header_words(reinterpret_cast<uint16_t*>(data.data()), size / 2);
    std::span<uint16_t> body_words(reinterpret_cast<uint16_t*>(body.data()), body.size() / 2);
    uint32_t sum = std::accumulate(header_words.begin(), header_words.end(), 0);
    sum = std::accumulate(body_words.begin(), body_words.end(), sum);
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    header_checksum(~sum);
  }
};

///@brief Mockup IPv4 UDP header.
//
// UDP wire format:
//
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-------+-------+-------+-------+-------+-------+-------+------+   ---
// |          source port          |      destination port        |    ^
// +-------+-------+-------+-------+-------+-------+-------+------+  8 bytes
// |            length             |          checksum            |    v
// +-------+-------+-------+-------+-------+-------+-------+------+   ---
// /                        data (if any)                         /
// +-------+-------+-------+-------+-------+-------+-------+------+

struct udp_header {
  constexpr static std::size_t size = 8;

  std::span<uint8_t, 8> data;

  explicit udp_header(const std::span<uint8_t, 8> &data) : data(data) {}

  inline void source_port(uint16_t value) { data[0] = value >> 8; data[1] = value & 0xFF; }
  inline void destination_port(uint16_t value) { data[2] = value >> 8; data[3] = value & 0xFF; }
  inline void length(uint16_t value) { data[4] = value >> 8; data[5] = value & 0xFF; }
  inline void checksum(uint16_t value) { data[6] = value >> 8; data[7] = value & 0xFF; }

  void calculate_checksum(const std::span<uint8_t>& body) {
    throw std::runtime_error("Not implemented");
  }
};

#endif //PROTOCOLS_H
