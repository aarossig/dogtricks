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

#ifndef DOGTRICKS_TRANSPORT_H_
#define DOGTRICKS_TRANSPORT_H_

#include <cstddef>
#include <cstdint>

#include "non_copyable.h"

namespace dogtricks {

/**
 * This class handles framing of messages to the satellite radio.
 */
class Transport : public NonCopyable {
 public:
  /**
   * The event handler for the transport to notify the application layers of
   * status changes.
   */
  class EventHandler {
   public:
    /**
     * Invoked when the transport has received a packet. This may not be the
     * packet immediately expected if a put message is sent between sending
     * a command and receiving the response.
     */
    virtual void OnPacketReceived() = 0;
  };

  /**
   * Setup the transport with the supplied link to perform tx/rx with.
   */
  Transport(const char *path, EventHandler& event_handler);

 private:
  //! The file descriptor used to communicate with the serial device.
  int fd_;

  //! The event handler for the transport.
  EventHandler& event_handler_;
};

}  // namespace dogtricks

#endif  // DOGTRICKS_TRANSPORT_H_
