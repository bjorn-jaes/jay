//
// Copyright (c) 2022 Bjørn Fuglestad, Jaersense AS (bjorn@jaersense.no)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/bjorn-jaes/jay
//

#ifndef JAY_NETWORK_MANAGER_H
#define JAY_NETWORK_MANAGER_H

#pragma once

// C++

// Lib

// Local
#include "address_manager.hpp"

namespace jay {

class network_manager
{
public:
  /**
   * @brief Construct a new network manager object
   * @param network
   */
  network_manager(jay::network &network) : network_(network) {}

  /**
   * @brief Construct a new network manager object
   * @param network
   * @param on_new_controller callback when new controllers / ecu s are added
   */
  network_manager(jay::network &network, std::function<void(jay::name, std::uint8_t)> on_new_controller)
    : network_(network), on_new_controller_(on_new_controller)
  {}

  void set_callback(std::function<void(jay::name, std::uint8_t)> on_new_controller)
  {
    on_new_controller_ = on_new_controller;
  }

  /**
   * @brief Inserts address manager into internal map
   * @param addr_man to insert
   */
  void insert(jay::address_manager &addr_man) { name_manager_map.insert({ addr_man.get_name(), addr_man }); }

  /**
   * @brief Processes address claim and address request frames
   * by tuning them into events and passing them to the state machine
   * @note also registes new controllers into the newtork and updates their address
   * @param frame containing and address claim or address request, other frames are ignored
   */
  void process(const jay::frame &frame)
  {
    if (frame.header.is_claim()) {
      on_frame_address_claim(jay::name(frame.payload), frame.header.pdu_specific(), frame.header.source_adderess());
      return;
    }

    if (frame.header.is_request()) { on_frame_address_request(frame.header.pdu_specific()); }
  }

  /**
   * @brief Number of controller addresses being managed
   * @return std::size_t
   */
  std::size_t size() const { return name_manager_map.size(); }

  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//
  //@                On Event callbacks implemntation                @//
  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//

private:
  /**
   * @internal
   * @brief Inserts address claims into network and converts claim to state machine event
   *
   * @param name
   * @param source_adderess
   */
  void on_frame_address_claim(jay::name name, std::uint8_t pdu_specific, std::uint8_t source_adderess)
  {
    auto in = network_.in_network(name);
    network_.insert(name, source_adderess);
    if (!in) {// Insert controller then notify with callback
      if (on_new_controller_) { on_new_controller_(name, source_adderess); }
    }

    jay::address_claimer::ev_address_claim claim{ name, source_adderess };

    /// NOTE: While it is allowed to send address claims to a specific address
    /// in allmost all cases it should be addressed to global 255

    if (pdu_specific < J1939_IDLE_ADDR) {
      if (auto name = network_.get_name(pdu_specific); name.has_value()) {// Claim targeted at specific address
        if (auto it = name_manager_map.find(name.value()); it != name_manager_map.end()) {
          it->second.address_claim(claim);
        }
      }
      return;
    }

    for (auto &ctrl : name_manager_map) { ctrl.second.address_claim(claim); }
  }

  /**
   * @brief Converts claim to state machine event
   * @param address
   */
  void on_frame_address_request(std::uint8_t address)
  {
    jay::address_claimer::ev_address_request req{};

    if (address < J1939_IDLE_ADDR) {
      if (auto name = network_.get_name(address); name.has_value()) {// Request address claim from specific address
        if (auto it = name_manager_map.find(name.value()); it != name_manager_map.end()) {
          it->second.address_request(req);
        }
      }
      return;
    }

    for (auto &ctrl : name_manager_map) { ctrl.second.address_request(req); }
  }


private:
  jay::network &network_;
  std::function<void(jay::name, std::uint8_t)> on_new_controller_;
  std::unordered_map<name_t, address_manager &> name_manager_map{};
};


}// namespace jay

#endif