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

#include <condition_variable>
#include <mutex>

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
   * The possible power states of the radio.
   */
  enum class PowerState : uint8_t {
    //! Put the receiver in sleep mode. This is reduced power and does not
    //! support receiving broadcast audio.
    SleepMode = 0x00,

    //! Power on the receiver. This mode permits receiving broadcast audio.
    FullMode = 0x03,
  };

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
   * @return true when stopped, false if the transport is not open.
   */
  bool Start();

  /**
   * Stops the receive loop in the radio object. This causes the previous
   * call to Start() to return true.
   */
  void Stop();

  /**
   * @return true if the transport was opened successfully. This must be
   *         queried before other commands can be sent to the radio.
   */
  bool IsOpen() const {
    return transport_.IsOpen();
  }

  /**
   * Sets the power state of the radio.
   *
   * @param power_state The power state to set to.
   */
  bool SetPowerMode(PowerState power_state);

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
  virtual void OnPacketReceived(Transport::OpCode op_code,
                                const uint8_t *payload,
                                size_t payload_size) override;

 private:
  //! The underlying transport to send/receive messages with.
  Transport transport_;

  //! The mutex to lock shared state.
  std::mutex mutex_;

  //! The condition variable used to resume a waiting command.
  std::condition_variable cv_;

  //! The expected response to the current outstanding message.
  Transport::OpCode response_op_code_;

  //! The response buffer to populate with the next response if matching.
  uint8_t *response_;

  //! The size of the response buffer to populate.
  size_t response_size_;

  /**
   * Sends a comment through the transport and populates the response buffer if
   * supplied.
   *
   * @param request_op_code The request op code.
   * @param response_op_code The expected response op code.
   * @param command The command payload.
   * @param command_size The size of the command payload to send.
   * @param response The response to populate.
   * @param response_size The maximum size of the response.
   * @param timeout The amount of time to spend waiting for the response.
   * @return true if successful, false on timeout.
   */
  bool SendCommand(Transport::OpCode request_op_code,
                   Transport::OpCode response_op_code,
                   const uint8_t *command, size_t command_size,
                   uint8_t *response, size_t response_size,
                   std::chrono::milliseconds timeout);
};

}  // namespace dogtricks

#endif  // DOGTRICKS_RADIO_H_
