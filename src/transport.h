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
   * The operation codes that are used when communicating with the radio.
   */
  enum class OpCode : uint16_t {
    SetPowerModeRequest = 0x0008,
    SetPowerModeResponse = 0x2008,
    SetResetRequest = 0x0009,
    SetResetResponse = 0x2009,
    SetChannelRequest = 0x000a,
    SetChannelResponse = 0x200a,
    GetSignalRequest = 0x4018,
    GetSignalResponse = 0x6018,
    PutModuleReadyResponse = 0x8000,
  };

  /**
   * Possible status codes returned by the radio.
   */
  enum class Status : uint16_t {
    Success = 0,
  };

  /**
   * Unpacks a uint16_t from the supplied buffer. The length is assumed to be
   * at least two.
   */
  static uint16_t UnpackUInt16(uint8_t *buffer) {
    return ((buffer[1] << 8) | buffer[0]);
  }

  /**
   * Unpacks a status from the supplied buffer. The length is assumed to be
   * at least two.
   */
  static Status UnpackStatus(uint8_t *buffer) {
    return static_cast<Status>(UnpackUInt16(buffer));
  }

  /**
   * Obtains a string description for a supplied signal strength byte.
   */
  static const char *GetSignalDescription(uint8_t value) {
    switch (value) {
      case 0:
        return "none";
      case 1:
        return "weak";
       case 2:
        return "good";
      case 3:
        return "excellent";
      default:
        return "<invalid>";
    }
  }

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
    virtual void OnPacketReceived(OpCode op_code, const uint8_t *payload,
                                  size_t payload_size) = 0;
  };

  /**
   * Setup the transport with the supplied link to perform tx/rx with.
   */
  Transport(const char *path, EventHandler& event_handler);

  /**
   * Clean up after the transport. Close the file for the serial device.
   */
  ~Transport();

  /**
   * Starts reception of frames from the device.
   *
   * @return true if the transport is open, false otherwise.
   */
  bool Start();

  /**
   * @return true if this transport was opened successfully.
   */
  bool IsOpen() const {
    return (fd_ > 0);
  }

  /**
   * Stops reception of messages from the device.
   */
  void Stop();

  /**
   * Sends a frame to the radio with the supplied attributes.
   *
   * @param op_code The op code to send.
   * @param payload The payload to send.
   * @param size The size of the command to send.
   */
  void SendMessageFrame(OpCode op_code, const uint8_t *payload, size_t size);

  /**
   * Receives a frame from the radio. This is a blocking call. The
   * OnPacketReceived event handler is invoked when a frame is read.
   */
  void ReceiveFrame();

 private:
  //! The size of the message buffer.
  static constexpr size_t kMessageBufferSize = UINT8_MAX + 32;

  //! The size of the tx/rx frame buffers.
  static constexpr size_t kTxRxBufferSize = UINT8_MAX + 64;

  //! The sync byte used to indicate a start of message.
  static constexpr uint8_t kSyncByte = 0xa4;

  //! The escape byte used to encode a sync.
  static constexpr uint8_t kEscapeByte = 0x1b;

  //! The byte to send when encoding an enscaped sync byte.
  static constexpr uint8_t kEscapedSyncByte = 0x53;

  //! The fixed byte to indicate the protocol version.
  static constexpr uint8_t kProtocolByte = 0x03;

  //! The value for a message frame.
  static constexpr uint8_t kMessageFrame = 0x00;

  //! The value for an Ack frame.
  static constexpr uint8_t kAckFrame = 0x80;

  //! The file descriptor used to communicate with the serial device.
  int fd_;

  //! Set to true when the transport is receiving frames.
  bool receiving_;

  //! The event handler for the transport.
  EventHandler& event_handler_;

  //! The next sequence number to use when sending a message payload. This
  //! increments and wraps across 255. This is required as it seems the device
  //! does not handle a fixed sequence number.
  uint8_t sequence_number_ = 0;

  /**
   * TODO: Docs.
   */
  void SendAckFrame(uint8_t sequence_number);

  /**
   * TODO: Docs.
   */
  void SendFrame(const uint8_t *frame, size_t size);

  /**
   * Inserts an escaped byte into the buffer.
   */
  bool InsertByte(uint8_t byte, uint8_t *buffer, size_t *pos, size_t size);

  /**
   * Computes the checksum of the frame in wire-format.
   */
  int8_t ComputeSum(const uint8_t *buffer, size_t size);

  /**
   * Reads a byte from the serial device. This may read multiple bytes if they
   * require escaping.
   */
  bool ReadByte(uint8_t *byte);

  /**
   * Reads one raw byte from the serial device.
   */
  bool ReadRawByte(uint8_t *byte);
};

}  // namespace dogtricks

#endif  // DOGTRICKS_TRANSPORT_H_
