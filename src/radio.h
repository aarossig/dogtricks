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
#include <optional>
#include <string>
#include <vector>

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
  //! A typedef for a list of channels.
  typedef std::vector<uint8_t> ChannelList;

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
   * The possible states for signal strength.
   */
  enum class SignalStrength : uint8_t {
    None = 0x00,
    Weak = 0x01,
    Good = 0x02,
    Excellent = 0x03,
  };

  /**
   * @return true if the supplied byte is a valid signal strength enumeration.
   */
  static bool SignalStrengthIsValid(uint8_t value) {
    return (static_cast<SignalStrength>(value) >= SignalStrength::None
        && static_cast<SignalStrength>(value) <= SignalStrength::Excellent);
  }

  /**
   * Obtains a string description for a supplied signal strength byte.
   */
  static const char *GetSignalDescription(SignalStrength value);

  /**
   * A grouping of metadata.
   *
   * The artist and title most frequently change and often contain
   * promotional content such as web URLs and phone numbers.
   */
  struct Metadata {
    //! Set when the artist changed.
    std::optional<std::string> artist;

    //! Set when the title changed.
    std::optional<std::string> title;

    //! Set when the album changed.
    std::optional<std::string> album;

    //! Set when the record label changes.
    std::optional<std::string> record_label;

    //! Set when the composer changes.
    std::optional<std::string> composer;

    //! Set when the alternate artist changes.
    std::optional<std::string> alt_artist;

    //! Set when the comments change.
    std::optional<std::string> comments;

    //! Promotional strings.
    std::vector<std::string> promo_text;
  };

  /**
   * A description of a channel.
   */
  struct ChannelDescriptor {
    //! The channel ID that this descriptor applies to. This may seem redundant,
    //! but consider if a collection of ChannelDescriptors were stored in an
    //! unordered container.
    uint8_t channel_id;

    //! The category ID of this channel.
    uint8_t category_id;

    //! The short name.
    std::string short_name;

    //! The long name.
    std::string long_name;

    //! The short category name.
    std::string short_category_name;

    //! The long category name.
    std::string long_category_name;

    //! The current metadata for the channel.
    Metadata metadata;
  };

  /**
   * Handles events from the radio such as status, metadata changes and
   * signal strength changes.
   */
  class EventHandler {
   public:
    /**
     * Inoked when the metadata for a channel has changed.
     */
    virtual void OnMetadataChange(uint8_t channel_id,
                                  const Metadata& event) = 0;
  };

  /**
   * Setup the radio object with the desired link.
   * 
   * @param path The path to the serial device to communicate with.
   * @param event_handler The event handler to invoke with radio events.
   */
  Radio(const char *path, EventHandler *event_handler) 
      : event_handler_(event_handler), transport_(path, *this) {}

  /**
   * Starts listening from the radio for packets if the transport was opened
   * successfully. This function blocks or returns false.
   *
   * @return true when stopped, false if the transport is not open.
   */
  bool Start() { return transport_.Start(); }

  /**
   * Stops the receive loop in the radio object. This causes the previous
   * call to Start() to return true.
   */
  void Stop() { transport_.Stop(); }

  /**
   * @return true if the transport was opened successfully. This must be
   *         queried before other commands can be sent to the radio.
   */
  bool IsOpen() const {
    return transport_.IsOpen();
  }

  /**
   * Issues a reset to the device.
   */
  bool Reset();

  /**
   * Sets the power state of the radio.
   *
   * @param power_state The power state to set to.
   */
  bool SetPowerMode(PowerState power_state);

  /**
   * Sets the channel to decode.
   *
   * @return true if successful, false otherwise.
   */
  bool SetChannel(uint8_t channel_id);

  /**
   * Sends a request for the current signal strength. Errors are logged.
   *
   * @param summary A signal strength pointer to fill in with a summary of the
   *                signal strength.
   * @param satellite A signal strength pointer to fill in with the satellite
   *                  signal strength.
   * @param terrestrial A signal strength pointer to fill in with the
   *                    terrestrial signal strength.
   * @return true if successful, false otherwise.
   */
  bool GetSignalStrength(SignalStrength *summary, SignalStrength *satellite,
                         SignalStrength *terrestrial);

  /**
   * Enables monitoring of metadata changes for all channels.
   *
   * @param enabled Whether or not to enable meta data monitoring.
   * @return true if successful, false otherwise.
   */
  bool SetGlobalMetadataMonitoringEnabled(bool enabled);

  /**
   * Reads the list of channels from the radio.
   *
   * @param channels A list that is populated with the available channels.
   * @return true if successful, false otherwise.
   */
  bool GetChannelList(ChannelList *channels);

 protected:
  // Transport::EventHandler methods.
  virtual void OnPacketReceived(Transport::OpCode op_code,
                                const uint8_t *payload,
                                size_t payload_size) override;

 private:
  /**
   * The various types of metadata that can be sent.
   */
  enum class MetadataType : uint8_t {
    Artist = 0x01,
    Title = 0x02,
    Album = 0x03,
    RecordLabel = 0x04,
    Composer = 0x06,
    AltArtist = 0x07,
    Comments = 0x08,
    PromoText1 = 0x20,
    PromoText2 = 0x21,
    PromoText3 = 0x22,
    PromoText4 = 0x23,
    SongId = 0x86,
    ArtistId = 0x88,
    Empty = 0xe0,
  };

  /**
   * Possible status codes returned by the radio.
   */
  enum class Status : uint16_t {
    Success = 0,
  };

  /**
   * Unpacks a status from the supplied buffer. The length is assumed to be
   * at least two.
   */
  static Status UnpackStatus(const uint8_t *buffer) {
    return static_cast<Status>(Transport::UnpackUInt16(buffer));
  }

  //! The event handler to invoke with radio state changes.
  EventHandler *event_handler_;

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

  //! Set to true when metadata monitoring is enabled.
  bool global_metadata_monitoring_enabled_ = false;

  /**
   * Sets the monitoring state based on the current configuration.
   *
   * @return true if successful, false otherwise.
   */
  bool SetMonitoringState();

  /**
   * Parses a metadata packet and posts an event to the event handler with the
   * change in state.
   *
   * @param payload The payload to parse.
   * @param size The size of the payload to parse.
   */
  void HandleMetadataPacket(const uint8_t *payload, size_t size);

  /**
   * Populates a field within a metadata with the supplied string and type.
   *
   * @param data The data to populate.
   * @param str_type Cooresponds to Transport::MetadataType, the type of the
   *                 string.
   * @param str The string to attach to the supplied event.
   */
  void PopulateMetadataEventField(Metadata *data,
                                  uint8_t str_type, std::string str);

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

  /**
   * Waits for the supplied put command and populates the put buffer if
   * supplied.
   *
   * @param put_op_code The put op code.
   * @param put The buffer to populate with the contents of the put.
   * @param put_size The size of the put buffer.
   * @param timeout The amount of time to spend waiting for the put.
   * @return true if successful, false on timeout.
   */
  bool WaitPut(Transport::OpCode put_op_code,
               uint8_t *put, size_t put_size,
               std::chrono::milliseconds timeout);
};

}  // namespace dogtricks

#endif  // DOGTRICKS_RADIO_H_
