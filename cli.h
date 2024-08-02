//
// Created by baloot on 22/07/24.
//

#ifndef CLI_H
#define CLI_H

#include <boost/program_options.hpp>
#include <boost/asio/ip/udp.hpp>
#include <optional>
#include <iostream>

namespace po = boost::program_options;

struct operation_arguments {
  uint16_t near_listen_port;
  boost::asio::ip::udp::endpoint near_endpoint;
  std::vector<uint16_t> far_listen_ports;
  std::vector<boost::asio::ip::udp::endpoint> far_endpoints;
  std::vector<boost::asio::ip::udp::endpoint> spoofed_endpoints;
  int replication_factor;
};

std::optional<operation_arguments> parse_arguments(int argc, char *argv[]) {
  po::options_description desc("Options");
  desc.add_options()
    ("nl", po::value<uint16_t>(), R"(
    <port> 
    The application on the near side should send packets to / receive packets from this port)")
    ("ne", po::value<std::string>(), R"(
    <ip>:<port>
    The application endpoint on the near side)")
    ("fl", po::value<std::vector<std::string>>(), R"(
    <port>[-<port>] [...]
    Magicmirror pair on the far side should send packets to these ports)")
    ("fe", po::value<std::vector<std::string>>(), R"(
    <ip[/mask]>:<port>[-<port>] [...]
    The endpoints of the magicmirror pair on the far side)")
    ("s", po::value<std::vector<std::string>>(), R"(
    <ip[/mask]>:<port>[-<port>] [...]
    IPs and ports to be used for spoofing)")
    ("r", po::value<int>(), R"(
    <number>
    The number of times each packet should be sent to the far side)")
    ("h,help", "Print help message");

  po::variables_map vm;
  try {
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
  } catch (const po::error& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return std::nullopt;
  }

  if (vm.count("h")) {
    std::cout << desc << std::endl;
    return std::nullopt;
  }

  if (!vm.count("nl") || !vm.count("ne") || !vm.count("fl") || !vm.count("fe") || !vm.count("s") || !vm.count("r")) {
    std::cerr << "Missing required arguments" << std::endl;
    return std::nullopt;
  }

  operation_arguments result;

  result.near_listen_port = vm["nl"].as<uint16_t>();
  std::cout << "Near listen port: " << result.near_listen_port << std::endl;

  std::string near_endpoint_str = vm["ne"].as<std::string>();
  std::size_t colon_pos = near_endpoint_str.find(':');
  std::string near_ip = near_endpoint_str.substr(0, colon_pos);
  uint16_t near_port = std::stoi(near_endpoint_str.substr(colon_pos + 1));
  result.near_endpoint = boost::asio::ip::udp::endpoint(boost::asio::ip::make_address_v4(near_ip), near_port);
  std::cout << "Near endpoint: " << result.near_endpoint << std::endl;

  std::vector<std::string> far_listen_strs = vm["fl"].as<std::vector<std::string>>();
  for (const auto& far_listen_str : far_listen_strs) {
    std::size_t dash_pos = far_listen_str.find('-');
    if (dash_pos == std::string::npos) {
      result.far_listen_ports.push_back(std::stoi(far_listen_str));
    } else {
      uint16_t far_listen_port_start = std::stoi(far_listen_str.substr(0, dash_pos));
      uint16_t far_listen_port_end = std::stoi(far_listen_str.substr(dash_pos + 1));
      for (uint16_t i = far_listen_port_start; i <= far_listen_port_end; i++) {
        result.far_listen_ports.push_back(i);
      }
    }
  }
  std::cout << "Far listen ports ";
  for (const auto& far_listen_port : result.far_listen_ports) {
    std::cout << far_listen_port << " ";
  }
  std::cout << std::endl;

  std::vector<std::string> far_endpoints_strs = vm["fe"].as<std::vector<std::string>>();
  for (const auto& far_endpoint_str : far_endpoints_strs) {
    uint32_t ip_start = 0;
    uint32_t ip_end = 0;
    uint16_t port_start = 0;
    uint16_t port_end = 0;
    std::size_t colon_pos = far_endpoint_str.find(':');

    std::string ip_str = far_endpoint_str.substr(0, colon_pos);
    std::size_t slash_pos = ip_str.find('/');
    if (slash_pos != std::string::npos) {
      uint32_t mask = (1 << (32 - std::stoi(ip_str.substr(slash_pos + 1)))) - 1;
      ip_start = (boost::asio::ip::make_address_v4(ip_str.substr(0, slash_pos)).to_ulong() & ~mask);
      ip_end = (boost::asio::ip::make_address_v4(ip_str.substr(0, slash_pos)).to_ulong() | mask);
    } else {
      ip_start = boost::asio::ip::make_address_v4(ip_str).to_ulong();
      ip_end = ip_start;
    }

    std::string port_str = far_endpoint_str.substr(colon_pos + 1);
    std::size_t dash_pos = port_str.find('-');
    if (dash_pos == std::string::npos) {
      port_start = std::stoi(port_str);
      port_end = port_start;
    } else {
      port_start = std::stoi(port_str.substr(0, dash_pos));
      port_end = std::stoi(port_str.substr(dash_pos + 1));
    }
  }
  std::cout << "Far endpoints: ";
  for (const auto& endpoint : result.far_endpoints) {
    std::cout << endpoint << " ";
  }
  std::cout << std::endl;

  std::string spoofed_ip_mask_range = vm["s"].as<std::string>();
  std::size_t slash_pos = spoofed_ip_mask_range.find('/');
  std::string spoofed_ip_str = spoofed_ip_mask_range.substr(0, slash_pos);
  uint32_t spoofed_ip_mask = (1 << (32 - std::stoi(spoofed_ip_mask_range.substr(slash_pos + 1)))) - 1;
  result.spoofed_ip_start = (boost::asio::ip::make_address_v4(spoofed_ip_str).to_ulong() & ~spoofed_ip_mask);
  result.spoofed_ip_end = (boost::asio::ip::make_address_v4(spoofed_ip_str).to_ulong() | spoofed_ip_mask);
  std::cout << "Spoofed IP range: " << boost::asio::ip::address_v4(result.spoofed_ip_start) << " - " << boost::asio::ip::address_v4(result.spoofed_ip_end) << std::endl;

  std::string spoofed_port_range = vm["s"].as<std::string>();
  std::size_t port_dash_pos = spoofed_port_range.find('-');
  result.spoofed_port_start = std::stoi(spoofed_port_range.substr(0, port_dash_pos));
  result.spoofed_port_end = std::stoi(spoofed_port_range.substr(port_dash_pos + 1));
  std::cout << "Spoofed port range: " << result.spoofed_port_start << " - " << result.spoofed_port_end << std::endl;

  result.replication_factor = vm["r"].as<int>();
  std::cout << "Replication factor: " << result.replication_factor << std::endl;

  return result;
}

#endif //CLI_H
