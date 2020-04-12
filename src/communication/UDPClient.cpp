// this is for emacs file handling -*- mode: c++; indent-tabs-mode: nil -*-

// -- BEGIN LICENSE BLOCK ----------------------------------------------

/*!
*  Copyright (C) 2018, SICK AG, Waldkirch
*  Copyright (C) 2018, FZI Forschungszentrum Informatik, Karlsruhe, Germany
*
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*    http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.

*/

// -- END LICENSE BLOCK ------------------------------------------------

//----------------------------------------------------------------------
/*!
 * \file AsyncUDPClient.cpp
 *
 * \author  Lennart Puck <puck@fzi.de>
 * \date    2018-09-24
 */
//----------------------------------------------------------------------

#include "sick_safetyscanners_base/communication/UDPClient.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <boost/lambda/bind.hpp>
#include <boost/lambda/lambda.hpp>

namespace sick
{
namespace communication
{

using boost::asio::deadline_timer;
using boost::asio::ip::tcp;
using boost::lambda::_1;
using boost::lambda::bind;
using boost::lambda::var;

UDPClient::UDPClient(
    boost::asio::io_service &io_service,
    unsigned short server_port)
    : m_io_service(io_service),
      m_socket(boost::ref(io_service), boost::asio::ip::udp::endpoint{boost::asio::ip::udp::v4(), server_port}),
      m_packet_handler(),
      m_recv_buffer(),
      m_deadline(io_service)
{
  m_deadline.expires_at(boost::posix_time::pos_infin);
  checkDeadline();
}

UDPClient::~UDPClient()
{
}

void UDPClient::checkDeadline()
{
  // Check whether the deadline has passed. We compare the deadline against
  // the current time since a new asynchronous operation may have moved the
  // deadline before this actor had a chance to run.
  if (m_deadline.expires_at() <= deadline_timer::traits_type::now())
  {
    // The deadline has passed. The socket is closed so that any outstanding
    // asynchronous operations are cancelled. This allows the blocked
    // connect(), read_line() or write_line() functions to return.
    boost::system::error_code ignored_ec;
    m_socket.close(ignored_ec);

    // There is no longer an active deadline. The expiry is set to positive
    // infinity so that the actor takes no action until a new deadline is set.
    m_deadline.expires_at(boost::posix_time::pos_infin);
  }

  // Put the actor back to sleep.
  m_deadline.async_wait(bind(&UDPClient::checkDeadline, this));
}

void UDPClient::handleReceive(boost::system::error_code ec, std::size_t bytes_recv)
{
  if (!ec)
  {
    sick::datastructure::PacketBuffer packet_buffer(m_recv_buffer, bytes_recv);
    std::cout << "packet_buffer ^^" << std::endl;
    m_packet_handler(packet_buffer);
  }
  else
  {
    LOG_ERROR("Error in UDP handle receive: %i", ec.value());
  }
  beginReceive();
}

void UDPClient::beginReceive()
{
  m_socket.async_receive_from(boost::asio::buffer(m_recv_buffer), m_remote_endpoint, [this](boost::system::error_code ec, std::size_t bytes_recvd) {
    this->handleReceive(ec, bytes_recvd);
  });
}

void UDPClient::stop()
{
  m_socket.cancel();
}

void UDPClient::connect()
{
  boost::system::error_code ec = boost::asio::error::host_not_found;

  if (ec)
  {
    LOG_ERROR("Could not connect to Sensor (UDP). Error code %i", ec.value());
    throw boost::system::system_error(ec);
  }
  else
  {
    LOG_INFO("UDP connection successfully established.");
  }
}

std::size_t UDPClient::receive(sick::datastructure::PacketBuffer &buffer, boost::posix_time::time_duration timeout)
{
  boost::system::error_code ec = boost::asio::error::would_block;
  m_deadline.expires_from_now(timeout);

  std::size_t bytes_recv = 0;
  m_socket.async_receive_from(boost::asio::buffer(m_recv_buffer), m_remote_endpoint,
                              boost::bind(&UDPClient::handleReceiveDeadline, _1, _2, &ec, &bytes_recv));

  // Block until async_receive_from finishes or the deadline_timer exceeds its timeout.
  do
    m_socket.get_io_service().run_one();
  while (ec == boost::asio::error::would_block);

  if (ec || !m_socket.is_open())
  {
    throw boost::system::system_error(ec ? ec : boost::asio::error::operation_aborted);
  }

  buffer = sick::datastructure::PacketBuffer(m_recv_buffer, bytes_recv);
  return bytes_recv;
}

bool UDPClient::isDataAvailable() const
{
  return m_socket.is_open() && m_socket.available() > 0;
}

void UDPClient::disconnect()
{
}

// TODO optional?
unsigned short UDPClient::getLocalPort() const
{
  if (m_socket.is_open())
  {
    return m_socket.local_endpoint().port();
  }
  return 0;
}

bool UDPClient::isConnected() const
{
  return m_socket.is_open();
}

} // namespace communication
} // namespace sick
