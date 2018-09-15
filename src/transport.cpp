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

#include "transport.h"

#include <cstring>

#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include "log.h"

namespace dogtricks {

Transport::Transport(const char *path, EventHandler& event_handler)
    : event_handler_(event_handler) {
  fd_ = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd_ < 0) {
    LOGE("Error opening device: %s (%d)", strerror(errno), errno);
  } else {
    LOGD("Serial device opened");

    // Configure the UART.
    struct termios options;
    memset(&options, 0, sizeof(struct termios));
    cfmakeraw(&options);

    if (cfsetspeed(&options, 115200) < 0) {
      LOGE("Error setting speed");
    } else {
      options.c_cflag |= CS8 | CLOCAL | CREAD;
      options.c_iflag = IGNPAR;
      options.c_cc[VMIN] = 0;
      options.c_cc[VTIME] = 10;
      if (tcsetattr(fd_, TCSANOW, &options) < 0) {
        LOGE("Failed to set serial port attributes");
      } else {
        int flags = fcntl(fd_, F_GETFL, 0);
        if (fcntl(fd_, F_SETFL, flags & ~O_NONBLOCK) != 0) {
          LOGE("Failed to set serial port to blocking mode");
        }

#ifdef __APPLE__
        // HACK: It seems like reading/writing from a tty device too soon after
        // opening the device can cause failures to read/write. Insert a small
        // delay after finishing device initialization.
        usleep(100000);
#endif  // __APPLE__
      }
    }
  }
}

}  // namespace dogtricks
