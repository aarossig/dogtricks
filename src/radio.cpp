/*
 * Copyright 2018 Andrew Rossignol (andrew.rossignol@gmail.com)
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "radio.h"

#include <cassert>
#include <cinttypes>
#include <cstring>

#include "log.h"

using namespace std::chrono_literals;

namespace dogtricks {

bool Radio::Start() {
  return transport_.Start();
}

void Radio::Stop() {
  transport_.Stop();
}

bool Radio::Reset() {
  uint8_t response[2];
  bool success = SendCommand(
      Transport::OpCode::SetResetRequest,
      Transport::OpCode::SetResetResponse,
      nullptr, 0, response, sizeof(response), 100ms);
  if (success) {
    Transport::Status status = Transport::UnpackStatus(response);
    success = (status == Transport::Status::Success);
    if (!success) {
      LOGE("Reset request failed with 0x%04" PRIx16, status);
    } else {
      do {
        success = WaitPut(Transport::OpCode::PutModuleReadyResponse,
                          response, sizeof(response), 5000ms);
      } while (success && response[0] != 0);
    }
  }

  return success;
}

bool Radio::SetPowerMode(PowerState power_state) {
  uint8_t payload[] = { static_cast<uint8_t>(power_state), };
  uint8_t response[4];
  bool success = SendCommand(
      Transport::OpCode::SetPowerModeRequest,
      Transport::OpCode::SetPowerModeResponse,
      payload, sizeof(payload), response, sizeof(response), 100ms);
  if (success) {
  }

  return success;
}

bool Radio::SetChannel(uint8_t channel_id) {
  uint8_t payload[] = { channel_id, 0, 0, 0 };
  uint8_t response[100];
  bool success = SendCommand(
      Transport::OpCode::SetChannelRequest,
      Transport::OpCode::SetChannelResponse,
      payload, sizeof(payload), response, sizeof(response), 100ms);
  if (success) {

  }

  return success;
}

bool Radio::GetSignalStrength() {
  uint8_t response[6];
  bool success = SendCommand(
      Transport::OpCode::GetSignalRequest,
      Transport::OpCode::GetSignalResponse,
      nullptr, 0, response, sizeof(response), 100ms);
  if (success) {

  }

  return success;
}

void Radio::OnPacketReceived(Transport::OpCode op_code, const uint8_t *payload,
                             size_t payload_size) {
  if (op_code == response_op_code_) {
    // If the supplied response buffer is too small, this is an error and it
    // must be increased in size.
    assert(response_size_ >= payload_size);
    memcpy(response_, payload, payload_size);
    cv_.notify_one();
  } else {
    LOGD("Unhandled op code: 0x%04" PRIx16, op_code);
  }
}

bool Radio::SendCommand(Transport::OpCode request_op_code,
                        Transport::OpCode response_op_code,
                        const uint8_t *command, size_t command_size,
                        uint8_t *response, size_t response_size,
                        std::chrono::milliseconds timeout) {
  std::unique_lock<std::mutex> lock(mutex_);
  response_op_code_ = response_op_code;
  response_ = response;
  response_size_ = response_size;

  transport_.SendMessageFrame(request_op_code, command, command_size);
  bool timed_out = (cv_.wait_for(lock, timeout) == std::cv_status::timeout);
  if (timed_out) {
    LOGE("Request 0x%04" PRIx16 " timed out", request_op_code);
  }

  return !timed_out;
}

bool Radio::WaitPut(Transport::OpCode put_op_code,
                    uint8_t *put, size_t put_size,
                    std::chrono::milliseconds timeout) {
  std::unique_lock<std::mutex> lock(mutex_);
  response_op_code_ = put_op_code;
  response_ = put;
  response_size_ = put_size;

  bool timed_out = (cv_.wait_for(lock, timeout) == std::cv_status::timeout);
  if (timed_out) {
    LOGE("Put 0x%04" PRIx16 " timed out", put_op_code);
  }

  return !timed_out;
}

}  // namespace dogtricks
