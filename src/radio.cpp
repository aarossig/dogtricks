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

bool Radio::SetPowerMode() {
  constexpr uint16_t kSetPowerMode = 0x0008;
  uint8_t payload[] = { 0x03, };
  return SendCommand(kSetPowerMode, payload, sizeof(payload));
}

bool Radio::GetSignalStrength() {
  constexpr uint16_t kGetSignalOpCode = 0x4018;
  return SendCommand(kGetSignalOpCode);
}

bool Radio::SendCommand(uint16_t op_code,
                        const uint8_t *command, size_t command_size,
                        uint8_t *response, size_t response_size) {
  bool success = transport_.SendMessageFrame(op_code, command, command_size);
  if (success) {
    // TODO: Condition variable wait.
  }

  return success;
}

void Radio::OnPacketReceived(uint16_t op_code, const uint8_t *payload,
                             size_t payload_size) {
  LOGD("OnPacketReceived 0x%04" PRIx16, op_code);
  if (op_code == 0x8000) {
    LOGD("Status %" PRIx8, payload[0]);
  } else {
    for (size_t i = 0; i < payload_size; i++) {
      LOGD("%zu %" PRIx8, i, payload[i]);
    }
  }
  // TODO: Condition variable notify, or otherwise.
}

}  // namespace dogtricks
