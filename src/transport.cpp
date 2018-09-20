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
  fd_ = open(path, O_RDWR | O_NOCTTY);
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

Transport::~Transport() {
  if (IsOpen()) {
    close(fd_);
  }
}

bool Transport::Start() {
  receiving_ = (fd_ > 0);
  bool running = receiving_;
  while(receiving_) {
    ReceiveFrame();
  }

  return running;
}

void Transport::Stop() {
  receiving_ = false;
}

void Transport::SendMessageFrame(OpCode op_code, const uint8_t *payload,
                                 size_t size) {
  assert(size <= UINT8_MAX);

  // Setup the message header.
  size_t message_pos = 0;
  uint8_t message_buffer[kMessageBufferSize];
  message_buffer[message_pos++] = kSyncByte;
  message_buffer[message_pos++] = kProtocolByte;
  message_buffer[message_pos++] = 0x00;
  message_buffer[message_pos++] = sequence_number_++;
  message_buffer[message_pos++] = kMessageFrame;
  message_buffer[message_pos++] = static_cast<uint8_t>(size) + 2;

  message_buffer[message_pos++] = static_cast<uint16_t>(op_code) >> 8;
  message_buffer[message_pos++] = static_cast<uint16_t>(op_code);

  // Copy the payload into the message.
  memcpy(&message_buffer[message_pos], payload, size);
  message_pos += size;

  // Insert the checksum.
  int8_t checksum = ComputeSum(message_buffer, message_pos);
  message_buffer[message_pos++] = -checksum;

  SendFrame(message_buffer, message_pos);
}

void Transport::ReceiveFrame() {
  // Sync to the next frame.
  uint8_t message_buffer[kMessageBufferSize] = {};
  while (ReadRawByte(&message_buffer[0]) && message_buffer[0] != kSyncByte) {}

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
  if (success) {
    int8_t computed_sum = ComputeSum(message_buffer, pos - 1);
    int8_t received_sum = message_buffer[pos - 1];

    if (static_cast<uint8_t>(computed_sum + received_sum) != 0) {
      LOGE("Invalid checksum %" PRId8 " vs %" PRId8,
           computed_sum, received_sum);
    } else {
      uint8_t sequence_number = message_buffer[3];
      uint8_t frame_type = message_buffer[4];
      if (frame_type == kMessageFrame) {
        SendAckFrame(sequence_number);
        if (message_buffer[5] < 2) {
          LOGE("Frame with short payload %" PRIu8, message_buffer[5]);
        } else {
          auto op_code = static_cast<OpCode>(
              (message_buffer[6] << 8) | message_buffer[7]);
          uint8_t *payload = &message_buffer[8];
          size_t payload_size = message_buffer[5] - 2;
          event_handler_.OnPacketReceived(op_code, payload, payload_size);
        }
      } else if (frame_type == kAckFrame) {
        // TODO: Handle this and other Nack frames.
      } else {
        LOGD("Received frame type %" PRIu8, frame_type);
      }
    }
  }
}

void Transport::SendAckFrame(uint8_t sequence_number) {
  // Setup the message header.
  size_t message_pos = 0;
  uint8_t message_buffer[kMessageBufferSize];
  message_buffer[message_pos++] = kSyncByte;
  message_buffer[message_pos++] = kProtocolByte;
  message_buffer[message_pos++] = 0x00;
  message_buffer[message_pos++] = sequence_number;
  message_buffer[message_pos++] = kAckFrame;
  message_buffer[message_pos++] = 0;

  // Insert the checksum.
  int8_t checksum = ComputeSum(message_buffer, message_pos);
  message_buffer[message_pos++] = -checksum;
  SendFrame(message_buffer, message_pos);
}

void Transport::SendFrame(const uint8_t *frame, size_t size) {
  bool success = true;
  size_t tx_buffer_pos = 0;
  uint8_t tx_buffer[kTxRxBufferSize];
  tx_buffer[tx_buffer_pos++] = kSyncByte;
  for (size_t i = 1; success && i < size; i++) {
    success &= InsertByte(frame[i], tx_buffer, &tx_buffer_pos,
                          sizeof(tx_buffer));
    // If a byte fails to insert, this is programming error and the buffer must
    // be increased in size.
    assert(success);
  }

  if (success) {
    size_t write_pos = 0;
    while (write_pos < tx_buffer_pos) {
      ssize_t result = write(fd_, &tx_buffer[write_pos],
                             tx_buffer_pos - write_pos);
      if (result < 0) {
        FATAL_ERROR("Failed to write to serial device with %s (%d)",
                    strerror(errno), errno);
      } else {
        write_pos += result;
      }
    }
  }
}

bool Transport::InsertByte(uint8_t byte, uint8_t *buffer,
                           size_t *pos, size_t size) {
  bool success = true;
  if (byte == kSyncByte && (*pos + 1) < size) {
    buffer[(*pos)++] = kEscapeByte;
    buffer[(*pos)++] = kEscapedSyncByte;
  } else if (byte == kEscapeByte && (*pos + 1) < size) {
    buffer[(*pos)++] = kEscapeByte;
    buffer[(*pos)++] = kEscapeByte;
  } else if (*pos < size) {
    buffer[(*pos)++] = byte;
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
  bool success = ReadRawByte(byte);
  if (success) {
    if (*byte == kEscapeByte) {
      success = ReadRawByte(byte);
      if (success) {
        if (*byte == kEscapedSyncByte) {
          *byte = kSyncByte;
        } else if (*byte == kEscapeByte) {
          *byte = kEscapeByte;
        } else {
          // TODO: This is due to an invalid escape sequence received from
          // hardware. This failure should be propagated but this is simpler.
          FATAL_ERROR("Invalid escape sequence");
        }
      }
    }
  }

  return success;
}

bool Transport::ReadRawByte(uint8_t *byte) {
  ssize_t result = 0;
  while (result == 0 && receiving_) {
    result = read(fd_, byte, 1);
    if (result < 0) {
      FATAL_ERROR("Failed to read from serial device with %s (%d)",
                  strerror(errno), errno);
    }
  }

  return receiving_;
}

}  // namespace dogtricks
