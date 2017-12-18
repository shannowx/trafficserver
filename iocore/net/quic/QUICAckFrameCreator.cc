/** @file
 *
 *  A brief file description
 *
 *  @section license License
 *
 *  Licensed to the Apache Software Foundation (ASF) under one
 *  or more contributor license agreements.  See the NOTICE file
 *  distributed with this work for additional information
 *  regarding copyright ownership.  The ASF licenses this file
 *  to you under the Apache License, Version 2.0 (the
 *  "License"); you may not use this file except in compliance
 *  with the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include "I_EventSystem.h"
#include "QUICAckFrameCreator.h"
#include <algorithm>

int
QUICAckFrameCreator::update(QUICPacketNumber packet_number, bool acknowledgable)
{
  if (this->_packet_numbers.size() == MAXIMUM_PACKET_COUNT) {
    return -1;
  }

  this->_packet_numbers.push_back(packet_number);
  if (acknowledgable && !this->_can_send) {
    this->_can_send = true;
  }

  return 0;
}

std::unique_ptr<QUICAckFrame, QUICFrameDeleterFunc>
QUICAckFrameCreator::create()
{
  std::unique_ptr<QUICAckFrame, QUICFrameDeleterFunc> ack_frame = QUICFrameFactory::create_null_ack_frame();
  if (this->_can_send) {
    ack_frame           = this->_create_ack_frame();
    this->_can_send     = false;
    this->_packet_count = 0;
    this->_packet_numbers.clear();
  }
  return ack_frame;
}

std::unique_ptr<QUICAckFrame, QUICFrameDeleterFunc>
QUICAckFrameCreator::create_if_needed()
{
  // TODO What would be criteria?
  return this->create();
}

void
QUICAckFrameCreator::_sort_packet_numbers()
{
  // TODO Find more smart way
}

std::unique_ptr<QUICAckFrame, QUICFrameDeleterFunc>
QUICAckFrameCreator::_create_ack_frame()
{
  std::unique_ptr<QUICAckFrame, QUICFrameDeleterFunc> ack_frame = QUICFrameFactory::create_null_ack_frame();
  this->_packet_numbers.sort();
  QUICPacketNumber largest_ack_number = this->_packet_numbers.largest_ack_number();
  QUICPacketNumber last_ack_number    = largest_ack_number;

  size_t i        = 0;
  uint8_t gap     = 0;
  uint64_t length = 0;

  while (i < this->_packet_numbers.size()) {
    if (this->_packet_numbers[i] == last_ack_number) {
      last_ack_number--;
      length++;
      i++;
      continue;
    }

    if (ack_frame) {
      ack_frame->ack_block_section()->add_ack_block({static_cast<uint8_t>(gap - 1), length - 1});
    } else {
      uint16_t delay = (Thread::get_hrtime() - this->_packet_numbers.largest_ack_received_time()) / 1000; // TODO Milliseconds?
      ack_frame      = QUICFrameFactory::create_ack_frame(largest_ack_number, delay, length - 1);
    }

    gap             = last_ack_number - this->_packet_numbers[i];
    last_ack_number = this->_packet_numbers[i];
    length          = 0;
  }

  if (ack_frame) {
    ack_frame->ack_block_section()->add_ack_block({static_cast<uint8_t>(gap - 1), length - 1});
  } else {
    uint16_t delay = (Thread::get_hrtime() - this->_packet_numbers.largest_ack_received_time()) / 1000; // TODO Milliseconds?
    ack_frame      = QUICFrameFactory::create_ack_frame(largest_ack_number, delay, length - 1);
  }
  return ack_frame;
}

void
QUICAckPacketNumbers::push_back(QUICPacketNumber packet_number)
{
  if (packet_number > this->_largest_ack_number) {
    this->_largest_ack_received_time = Thread::get_hrtime();
    this->_largest_ack_number        = packet_number;
  }

  this->_packet_numbers.push_back(packet_number);
}

QUICPacketNumber
QUICAckPacketNumbers::front()
{
  return this->_packet_numbers.front();
}

QUICPacketNumber
QUICAckPacketNumbers::back()
{
  return this->_packet_numbers.back();
}

size_t
QUICAckPacketNumbers::size()
{
  return this->_packet_numbers.size();
}

void
QUICAckPacketNumbers::clear()
{
  this->_packet_numbers.clear();
  this->_largest_ack_number        = 0;
  this->_largest_ack_received_time = 0;
}

QUICPacketNumber
QUICAckPacketNumbers::largest_ack_number()
{
  return this->_largest_ack_number;
}

ink_hrtime
QUICAckPacketNumbers::largest_ack_received_time()
{
  return this->_largest_ack_received_time;
}

void
QUICAckPacketNumbers::sort()
{
  //  TODO Find more smart way
  std::sort(this->_packet_numbers.begin(), this->_packet_numbers.end(),
            [](QUICPacketNumber a, QUICPacketNumber b) -> bool { return b < a; });
}