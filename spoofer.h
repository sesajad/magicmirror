//
// Created by sajad on 17/07/24.
//

#ifndef MAGICMIRROR_SPOOFER_H
#define MAGICMIRROR_SPOOFER_H


#include <string>
#include <bits/socket.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <unistd.h>
#include <cstring>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/basic_raw_socket.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/ip/udp.hpp>

#include "protocols.h"

using udp = boost::asio::ip::udp;

template<typename T>
using awaitable = boost::asio::awaitable<T>;

/// @brief raw socket provides the protocol for raw socket.
class raw
{
public:
  ///@brief The type of a raw endpoint.
  typedef boost::asio::ip::basic_endpoint<raw> endpoint;

  ///@brief The raw socket type.
  typedef boost::asio::basic_raw_socket<raw> socket;

  ///@brief The raw resolver type.
  typedef boost::asio::ip::basic_resolver<raw> resolver;

  ///@brief Construct to represent the IPv4 RAW protocol.
  static raw v4()
  {
    return raw(IPPROTO_RAW, PF_INET);
  }

  ///@brief Construct to represent the IPv6 RAW protocol.
  static raw v6()
  {
    return raw(IPPROTO_RAW, PF_INET6);
  }

  ///@brief Default constructor.
  explicit raw()
    : protocol_(IPPROTO_RAW),
      family_(PF_INET)
  {}

  ///@brief Obtain an identifier for the type of the protocol.
  int type() const
  {
    return SOCK_RAW;
  }

  ///@brief Obtain an identifier for the protocol.
  int protocol() const
  {
    return protocol_;
  }

  ///@brief Obtain an identifier for the protocol family.
  int family() const
  {
    return family_;
  }

  ///@brief Compare two protocols for equality.
  friend bool operator==(const raw& p1, const raw& p2)
  {
    return p1.protocol_ == p2.protocol_ && p1.family_ == p2.family_;
  }

  /// Compare two protocols for inequality.
  friend bool operator!=(const raw& p1, const raw& p2)
  {
    return !(p1 == p2);
  }

private:
  explicit raw(int protocol_id, int protocol_family)
    : protocol_(protocol_id),
      family_(protocol_family)
  {}

  int protocol_;
  int family_;
};

template <std::size_t body_buffer_size>
awaitable<void> send_spoofed_udp(raw::socket &socket, const udp::endpoint &from,
  const udp::endpoint &to, const std::span<uint8_t> &body) {
  std::array<uint8_t, ipv4_header::size + udp_header::size + body_buffer_size> buffer;
  std::span buffer_span = std::span(buffer).subspan(0, ipv4_header::size + udp_header::size + body.size());
  std::copy(body.begin(), body.end(), buffer_span.subspan(ipv4_header::size + udp_header::size).begin());

  udp_header udp(buffer_span.template subspan<ipv4_header::size, udp_header::size>());
  udp.source_port(from.port());
  udp.destination_port(to.port());
  udp.length(udp_header::size + body.size());
  udp.checksum(0);

  // Create the IPv4 header.
  ipv4_header ip(buffer_span.template first<ipv4_header::size>());
  ip.version(4);
  ip.header_length(ipv4_header::size / 4); // 32-bit words
  ip.type_of_service(0); // Differentiated service code point
  ip.identification(0);
  ip.dont_fragment(true);
  ip.more_fragments(false);
  ip.fragment_offset(0);
  ip.time_to_live(255);
  ip.source_address(from.address().to_v4());
  ip.destination_address(to.address().to_v4());
  ip.protocol(IPPROTO_UDP);
  ip.calculate_length(buffer_span.subspan(ipv4_header::size));
  ip.calculate_checksum(buffer_span.subspan(ipv4_header::size));

  auto bytes_transferred = co_await socket.async_send_to(boost::asio::buffer(buffer_span.data(), buffer_span.size()),
    raw::endpoint(to.address(), to.port()), boost::asio::use_awaitable);
  assert (bytes_transferred == buffer_span.size());
}

#endif //MAGICMIRROR_SPOOFER_H
