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

#include <cassert>
#include <cinttypes>
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

    if (cfsetspeed(&options, 57600) < 0) {
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

bool Transport::SendFrame(uint16_t op_code, const uint8_t *payload,
                          size_t size) {
  assert(size <= UINT8_MAX);

  // Setup the message header.
  size_t message_pos = 0;
  uint8_t message_buffer[kMessageBufferSize];
  message_buffer[message_pos++] = kSyncByte;
  message_buffer[message_pos++] = kProtocolByte;
  message_buffer[message_pos++] = 0x00; // TODO: Accept module ID?
  message_buffer[message_pos++] = 0x00; // TODO: Accept a sequence number?
  message_buffer[message_pos++] = kMessageFrame;
  message_buffer[message_pos++] = static_cast<uint8_t>(size);

  // Copy the payload into the message.
  memcpy(&message_buffer[message_pos], payload, size);
  message_pos += size;

  // Insert the checksum.
  message_buffer[message_pos++] = -ComputeSum(message_buffer, message_pos);
  
  bool success = false;
  return success;
}

bool Transport::ReceiveFrame() {
  // Sync to the next frame.
  uint8_t message_buffer[kMessageBufferSize] = {};
  while ((message_buffer[0] = ReadRawByte()) != kSyncByte) {}

  // Read the fields of the header.
  bool success = true;
  size_t pos = 1;
  success &= ReadByte(&message_buffer[pos++]);
  success &= ReadByte(&message_buffer[pos++]);
  success &= ReadByte(&message_buffer[pos++]);
  success &= ReadByte(&message_buffer[pos++]);
  success &= ReadByte(&message_buffer[pos++]);

  // Read the payload.
  for (size_t i = 0; success && i < message_buffer[5]; i++) {
    success &= ReadByte(&message_buffer[pos++]);
  }

  // Read the checksum.
  success &= ReadByte(&message_buffer[pos++]);

  // Compute the checksum.
  int8_t computed_sum = ComputeSum(message_buffer, pos - 1);
  int8_t received_sum = message_buffer[pos - 1];

  success = false;
  if (computed_sum + received_sum != 0) {
    LOGE("Invalid checksum");
  } else if (message_buffer[5] < 2) {
    LOGE("Frame with short payload");
  } else {
    uint16_t op_code = (message_buffer[6] << 8) | message_buffer[7];
    uint8_t *payload = &message_buffer[8];
    size_t payload_size = message_buffer[5] - 2;
    event_handler_.OnPacketReceived(op_code, payload, payload_size);
    success = true;
  }

  return success;
}

bool Transport::InsertByte(uint8_t byte, uint8_t *buffer,
                           size_t *pos, size_t size) {
  bool success = true;
  if (byte == kSyncByte && (*pos + 1) < size) {
    buffer[*pos++] = kEscapeByte;
    buffer[*pos++] = kEscapedSyncByte;
  } else if (byte == kEscapeByte && (*pos + 1) < size) {
    buffer[*pos++] = kEscapeByte;
    buffer[*pos++] = kEscapeByte;
  } else if (*pos < size) {
    buffer[*pos++] = byte;
  } else {
    success = false;
  }

  return success;
}

int8_t Transport::ComputeSum(const uint8_t *buffer, size_t size) {
  int8_t sum = 0;
  for (size_t i = 0; i < size; i++) {
    sum += static_cast<int8_t>(buffer[i]);
  }

  return sum;
}

bool Transport::ReadByte(uint8_t *byte) {
  bool success = true;
  uint8_t raw_byte = ReadRawByte();
  if (raw_byte == kEscapeByte) {
    raw_byte = ReadRawByte();
    if (raw_byte == kEscapedSyncByte) {
      *byte = kSyncByte;
    } else if (raw_byte == kEscapeByte) {
      *byte = kEscapeByte;
    } else {
      LOGE("Invalid escape sequence");
      success = false;
    }
  } else {
    *byte = raw_byte;
  }

  return success;
}

uint8_t Transport::ReadRawByte() {
  uint8_t byte;
  ssize_t result = 0;
  while (result == 0) {
    result = read(fd_, &byte, 1);
    if (result < 0) {
      FATAL_ERROR("Failed to ready from serial device with %s (%d)",
                  strerror(errno), errno);
    }
  }

  return byte;
}

}  // namespace dogtricks
