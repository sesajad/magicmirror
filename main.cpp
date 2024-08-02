#include <iostream>
#include <random>
#include <boost/asio/basic_raw_socket.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/ip/udp.hpp>

#include "spoofer.h"

template<typename T>
using awaitable = boost::asio::awaitable<T>;

using udp_socket = boost::asio::ip::udp::socket;
using raw_socket = boost::asio::basic_raw_socket<raw>;

awaitable<void> near_listen(udp_socket &near_socket, raw_socket &far_socket,
                            std::function<boost::asio::ip::udp::endpoint()> spoofed_endpoint_generator,
                            const boost::asio::ip::udp::endpoint &far_endpoint) {
  std::array<uint8_t, 65535> buffer_array;
  std::span buffer_span(buffer_array);
  boost::asio::mutable_buffer buffer(buffer_array.data(), buffer_array.size());

  for (;;) {
    const std::size_t result = co_await near_socket.async_receive(buffer, boost::asio::use_awaitable);
    co_await send_spoofed_udp<65535>(far_socket, spoofed_endpoint_generator(), far_endpoint,
      buffer_span.subspan(0, result));
  }
}

awaitable<void> far_listen(udp_socket &far_socket, udp_socket &near_socket, const boost::asio::ip::udp::endpoint &near_endpoint) {
  std::array<uint8_t, 1024> buffer;
  for (;;) {
    const std::size_t result = co_await far_socket.async_receive(boost::asio::buffer(buffer), boost::asio::use_awaitable);
    co_await near_socket.async_send_to(boost::asio::buffer(buffer, result), near_endpoint, boost::asio::use_awaitable);
    std::cout << "Received " << result << " bytes from far\n";
  }
}

int main(int argc, char *argv[]) {
  // magicmirror [near-port] [far-ip] [far-port] [spoofed-ip-mask] [spoofed-port-range]
  if (argc != 9) {
    std::cerr << "Usage: " << argv[0] << " [near-listen-port] [near-ip] [near-port] [far-listen-port-range] [far-ip] [far-port] [spoofed-ip-mask] [spoofed-port-range]\n";
    return 1;
  }

  uint16_t near_listen_port = std::stoi(argv[1]);
  std::cout << "Near listen port: " << near_listen_port << std::endl;

  boost::asio::ip::udp::endpoint near_endpoint(boost::asio::ip::make_address_v4(argv[2]),
    std::stoi(argv[3]));
  std::cout << "Near endpoint: " << near_endpoint << std::endl;

  uint16_t far_listen_port_start = std::stoi(std::string(argv[4]).substr(0, std::string(argv[4]).find('-')));
  uint16_t far_listen_port_end = std::stoi(std::string(argv[4]).substr(std::string(argv[4]).find('-') + 1));
  std::cout << "Far listen port range: " << far_listen_port_start << " - " << far_listen_port_end << std::endl;

  // TODO, get a list/range for far endpoints
  boost::asio::ip::udp::endpoint far_endpoint(boost::asio::ip::make_address_v4(argv[5]),
    std::stoi(argv[6]));
  std::cout << "Far endpoint: " << far_endpoint << std::endl;

  auto spoofed_ip = boost::asio::ip::make_address_v4(std::string(argv[7]).substr(0, std::string(argv[7]).find('/')));
  uint32_t spoofed_ip_mask = (1 << (32 - std::stoi(std::string(argv[7]).substr(std::string(argv[7]).find('/') + 1)))) - 1;

  uint32_t spoofed_ip_start = (spoofed_ip.to_ulong() & ~spoofed_ip_mask);
  uint32_t spoofed_ip_end = (spoofed_ip.to_ulong() | spoofed_ip_mask);
  std::cout << "Spoofed IP range: " << boost::asio::ip::address_v4(spoofed_ip_start) << " - " << boost::asio::ip::address_v4(spoofed_ip_end) << std::endl;

  uint16_t spoofed_port_start = std::stoi(std::string(argv[8]).substr(0, std::string(argv[8]).find('-')));
  uint16_t spoofed_port_end = std::stoi(std::string(argv[8]).substr(std::string(argv[8]).find('-') + 1));
  std::cout << "Spoofed port range: " << spoofed_port_start << " - " << spoofed_port_end << std::endl;

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<uint32_t> ip_dis(spoofed_ip_start, spoofed_ip_end);
  std::uniform_int_distribution<uint32_t> port_dis(spoofed_port_start, spoofed_port_end);

  auto spoofed_endpoint_generator = [&]() {
    return boost::asio::ip::udp::endpoint(boost::asio::ip::address_v4(ip_dis(gen)), port_dis(gen));
  };

  boost::asio::io_context io_context{};

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
