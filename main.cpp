#include <iostream>
#include <random>
#include <boost/asio/basic_raw_socket.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/ip/udp.hpp>

#include "spoofer.h"
#include "cli.h"

template<typename T>
using awaitable = boost::asio::awaitable<T>;

using udp_socket = boost::asio::ip::udp::socket;
using raw_socket = boost::asio::basic_raw_socket<raw>;

boost::asio::io_context io_context{};

awaitable<void> near_listen(udp_socket &near_socket, raw_socket &far_socket,
                            std::function<boost::asio::ip::udp::endpoint()> spoofed_endpoint_generator,
                            const boost::asio::ip::udp::endpoint &far_endpoint) {
  std::array<uint8_t, 65535> buffer_array;
  std::span buffer_span(buffer_array);
  boost::asio::mutable_buffer buffer(buffer_array.data(), buffer_array.size());

  for (;;) {
    const std::size_t result = co_await near_socket.async_receive(buffer, boost::asio::use_awaitable);
    auto send_coro = send_spoofed_udp<65535>(far_socket, spoofed_endpoint_generator(), far_endpoint,
      buffer_span.subspan(0, result));
    co_spawn(io_context, std::move(send_coro), boost::asio::detached);
  }
}

awaitable<void> far_listen(udp_socket &far_socket, udp_socket &near_socket, const boost::asio::ip::udp::endpoint &near_endpoint) {
  std::array<uint8_t, 1024> buffer;
  for (;;) {
    const std::size_t result = co_await far_socket.async_receive(boost::asio::buffer(buffer), boost::asio::use_awaitable);
    co_spawn(io_context, near_socket.async_send_to(boost::asio::buffer(buffer, result), near_endpoint, boost::asio::use_awaitable), boost::asio::detached);
  }
}

int main(int argc, char *argv[]) {
  auto action = parse_arguments(argc, argv);
  if (!action) {
    return 1;
  }

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<uint32_t> ip_dis(action->spoofed_ip_start, action->spoofed_ip_end);
  std::uniform_int_distribution<uint32_t> port_dis(spoofed_port_start, spoofed_port_end);

  auto spoofed_endpoint_generator = [&]() {
    return boost::asio::ip::udp::endpoint(boost::asio::ip::address_v4(ip_dis(gen)), port_dis(gen));
  }

  auto far_endpoint_generator = [&]() {
    return boost::asio::ip::udp::endpoint(boost::asio::ip::address_v4(ip_dis(gen)), port_dis(gen));
  }


  boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
  signals.async_wait([&](auto, auto){ io_context.stop(); });

  udp_socket near_socket(io_context, {boost::asio::ip::udp::v4(), near_listen_port});

  std::vector<udp_socket> far_receivers_sockets;
  assert (far_listen_port_end >= far_listen_port_start);
  for (uint16_t i = far_listen_port_start; i <= far_listen_port_end; i++) {
    far_receivers_sockets.emplace_back(io_context, udp::endpoint{boost::asio::ip::udp::v4(), i});
  }

  boost::asio::basic_raw_socket<raw> far_sender_socket(io_context,
                                            raw::endpoint(raw::v4(), 0));

  co_spawn(io_context, near_listen(near_socket, far_sender_socket, spoofed_endpoint_generator, far_endpoint), boost::asio::detached);
  for (auto &far_receiver_socket : far_receivers_sockets) {
    co_spawn(io_context, far_listen(far_receiver_socket, near_socket, near_endpoint), boost::asio::detached);
  }

  io_context.run();

  return 0;
}
