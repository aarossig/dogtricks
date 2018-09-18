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

#include <cinttypes>

#include "log.h"

namespace dogtricks {

bool Radio::Start() {
  bool success = transport_.IsOpen();
  if (!success) {
    LOGE("Failed to start, transport not open");
  }

  // TODO: Support a stop.
  while (success) {
    success = transport_.ReceiveFrame();
  }

  return success;
}

bool Radio::SetPowerMode(PowerState power_state) {
  uint8_t payload[] = { static_cast<uint8_t>(power_state), };
  uint8_t response[4];
  SendCommand(Transport::OpCode::SetPowerModeRequest,
              Transport::OpCode::SetPowerModeRequest,
              payload, sizeof(payload),
              response, sizeof(response));
  return true;
}

bool Radio::SetChannel(uint8_t channel_id) {
  uint8_t payload[] = { channel_id, 0, 0, 0 };
  uint8_t response[100];
  SendCommand(Transport::OpCode::SetChannelRequest,
              Transport::OpCode::SetChannelResponse,
              payload, sizeof(payload),
              response, sizeof(response));
  return true;
}

bool Radio::GetSignalStrength() {
  uint8_t response[6];
  SendCommand(Transport::OpCode::GetSignalRequest,
              Transport::OpCode::GetSignalResponse,
              nullptr, 0, response, sizeof(response));
  return true;
}

void Radio::SendCommand(Transport::OpCode request_op_code,
                        Transport::OpCode response_op_code,
                        const uint8_t *command, size_t command_size,
                        uint8_t *response, size_t response_size) {
  transport_.SendMessageFrame(request_op_code, command, command_size);

  // TODO: Condition variable stuff.
}

void Radio::OnPacketReceived(Transport::OpCode op_code, const uint8_t *payload,
                             size_t payload_size) {
  LOGD("OnPacketReceived 0x%04" PRIx16, op_code);

  for (size_t i = 0; i < payload_size; i++) {
    LOGD("%zu %" PRIx8, i, payload[i]);
  }
  // TODO: Condition variable notify, or otherwise.
}

}  // namespace dogtricks
