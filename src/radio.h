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

  // Transport::EventHandler methods.
  virtual void OnPacketReceived() override;

 private:
  //! The underlying transport to send/receive messages with.
  Transport transport_;
};

}  // namespace dogtricks

#endif  // DOGTRICKS_RADIO_H_
