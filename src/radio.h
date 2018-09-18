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

#ifndef DOGTRICKS_RADIO_H_
#define DOGTRICKS_RADIO_H_

#include "non_copyable.h"
#include "transport.h"

namespace dogtricks {

/**
 * The top level radio class that deals with application level commands and
 * messages to/from the radio.
 */
class Radio : public Transport::EventHandler,
              public NonCopyable {
 public:
  /**
   * Setup the radio object with the desired link.
   * 
   * @param path The path to the serial device to communicate with.
   */
  Radio(const char *path) : transport_(path, *this) {}

  /**
   * Starts listening from the radio for packets if the transport was opened
   * successfully. This function blocks or returns false.
   *
   * @return false if the transport is not open.
   */
  bool Start();

  /**
   *
   */
  bool SetPowerMode();

  /**
   *
   */
  bool SetChannel(uint8_t channel_id);

  /**
   * Sends a request for the current signal strength. Errors are logged.
   *
   * @return true if successful, false otherwise.
   */
  bool GetSignalStrength();

 protected:
  // Transport::EventHandler methods.
  virtual void OnPacketReceived(uint16_t op_code, const uint8_t *payload,
                                size_t payload_size) override;

 private:
  //! The underlying transport to send/receive messages with.
  Transport transport_;

  /**
   * Sends a comment through the transport and populates the response buffer if
   * supplied.
   *
   * @param op_code The op code to send.
   * @param command The command payload.
   * @param command_size The size of the command payload to send.
   * @param response The response to populate.
   * @param response_size The maximum size of the response.
   */
  bool SendCommand(uint16_t op_code,
                   const uint8_t *command = nullptr, size_t command_size = 0,
                   uint8_t *response = nullptr, size_t response_size = 0);
};

}  // namespace dogtricks

#endif  // DOGTRICKS_RADIO_H_
