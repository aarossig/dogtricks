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

#include <cassert>
#include <cinttypes>
#include <cstring>

#include "log.h"

using namespace std::chrono_literals;

// TODO: A rather glaring problem with this implementation is that the size of
// the received packet is completely ignored. This means that the radio could
// return malformed data resulting in an index out of bounds. This should be
// resolved with more rigorous bounds checking on packet parsing.

namespace dogtricks {

const char *Radio::GetSignalDescription(SignalStrength signal_strength) {
  switch (signal_strength) {
    case SignalStrength::None:
      return "none";
    case SignalStrength::Weak:
      return "weak";
    case SignalStrength::Good:
      return "good";
    case SignalStrength::Excellent:
      return "excellent";
    default:
      return nullptr;
  }
}

bool Radio::Reset() {
  uint8_t response[2];
  bool success = SendCommand(
      Transport::OpCode::SetResetRequest,
      Transport::OpCode::SetResetResponse,
      nullptr, 0, response, sizeof(response), 100ms);
  if (success) {
    auto status = UnpackStatus(response);
    success = (status == Status::Success);
    if (!success) {
      LOGE("Reset request failed with 0x%04" PRIx16, status);
    } else {
      do {
        success = WaitPut(Transport::OpCode::PutModuleReadyResponse,
                          response, sizeof(response), 5000ms);
      } while (success && response[0] != 0);
    }
  }

  return success;
}

bool Radio::SetPowerMode(PowerState power_state) {
  uint8_t payload[] = { static_cast<uint8_t>(power_state), };
  uint8_t response[4];
  bool success = SendCommand(
      Transport::OpCode::SetPowerModeRequest,
      Transport::OpCode::SetPowerModeResponse,
      payload, sizeof(payload), response, sizeof(response), 100ms);
  if (success) {
    auto status = UnpackStatus(response);
    success = (status == Status::Success);
  }

  return success;
}

bool Radio::SetChannel(uint8_t channel_id) {
  uint8_t payload[] = { channel_id, 0, 0, 0 };
  uint8_t response[UINT8_MAX];
  bool success = SendCommand(
      Transport::OpCode::SetChannelRequest,
      Transport::OpCode::SetChannelResponse,
      payload, sizeof(payload), response, sizeof(response), 100ms);
  if (success) {
    auto status = UnpackStatus(response);
    success = (status == Status::Success);
    if (!success) {
      LOGE("Set channel request failed with 0x%04" PRIx16, status);
    }
  }

  return success;
}

bool Radio::GetSignalStrength(SignalStrength *summary,
                              SignalStrength *satellite,
                              SignalStrength *terrestrial) {
  uint8_t response[6];
  bool success = SendCommand(
      Transport::OpCode::GetSignalRequest,
      Transport::OpCode::GetSignalResponse,
      nullptr, 0, response, sizeof(response), 100ms);
  if (success) {
    auto status = UnpackStatus(response);
    success = (status == Status::Success);
    if (!success) {
      LOGE("Get signal strength request failed with 0x%04" PRIx16, status);
    } else {
      success &= SignalStrengthIsValid(response[2]);
      success &= SignalStrengthIsValid(response[3]);
      success &= SignalStrengthIsValid(response[4]);
      *summary = static_cast<SignalStrength>(response[2]);
      *satellite = static_cast<SignalStrength>(response[3]);
      *terrestrial = static_cast<SignalStrength>(response[4]);
    } 
  }

  return success;
}

bool Radio::SetGlobalMetadataMonitoringEnabled(bool enabled) {
  global_metadata_monitoring_enabled_ = enabled;
  return SetMonitoringState();
}

bool Radio::GetChannelList(ChannelList *channels) {
  // List all channels.
  uint8_t request[] = {
      0 /* base channel */,
      1 /* upward */,
      224 /* count */,
      0 /* overrides */
  };
  uint8_t response[UINT8_MAX];
  bool success = SendCommand(
      Transport::OpCode::GetChannelListRequest,
      Transport::OpCode::GetChannelListResponse,
      request, sizeof(request), response, sizeof(response), 100ms);
  if (success) {
    auto status = UnpackStatus(response);
    success = (status == Status::Success);
    if (!success) {
      LOGE("Get channel list request failed with 0x%04" PRIx16, status);
    } else {
      uint8_t channel_count = response[2];
      for (uint8_t i = 0; i < channel_count; i++) {
        channels->push_back(response[3 + i]);
      }
    }
  }

  return success;
}

bool Radio::GetChannelDescriptor(uint8_t channel_id,
                                 ChannelDescriptor *descriptor) {
  uint8_t request[] = {
    channel_id,
    0 /* direction: direct */,
    0 /* use category: no */,
    0 /* overrides */,
  };
  uint8_t response[UINT8_MAX];
  bool success = SendCommand(
      Transport::OpCode::GetChannelRequest,
      Transport::OpCode::GetChannelResponse,
      request, sizeof(request), response, sizeof(response), 100ms);
  if (success) {
    auto status = UnpackStatus(response);
    success = (status == Status::Success);
    if (!success) {
      LOGE("Get channel request failed with 0x%04" PRIx16, status);
    } else {
      descriptor->channel_id = response[2];
      descriptor->category_id = response[4];

      size_t offset = 7;
      size_t length = response[offset++];
      descriptor->short_name = std::string(
          reinterpret_cast<const char *>(&response[offset]), length);

      offset += length;
      length = response[offset++];
      descriptor->long_name = std::string(
          reinterpret_cast<const char *>(&response[offset]), length);

      offset += length;
      length = response[offset++];
      descriptor->short_category_name = std::string(
          reinterpret_cast<const char *>(&response[offset]), length);

      offset += length;
      length = response[offset++];
      descriptor->long_category_name = std::string(
          reinterpret_cast<const char *>(&response[offset]), length);

      // TODO: Pass in the length of the packet. This is bypassed for now as
      // the sie of the response is not currently passed back after sending a
      // command.
      offset += length;
      ParseMetadata(&response[offset], SIZE_MAX, &descriptor->metadata);
    }
  }

  return success;
}

void Radio::OnPacketReceived(Transport::OpCode op_code, const uint8_t *payload,
                             size_t payload_size) {
  if (op_code == response_op_code_) {
    // If the supplied response buffer is too small, this is an error and it
    // must be increased in size.
    assert(response_size_ >= payload_size);
    memcpy(response_, payload, payload_size);
    cv_.notify_one();
  } else if (op_code == Transport::OpCode::PutPdtResponse) {
    if (global_metadata_monitoring_enabled_) {
      HandleMetadataPacket(payload, payload_size);
    } else {
      LOGD("Received unsolicited metadata change");
    }
  } else {
    LOGD("Unhandled op code: 0x%04" PRIx16, op_code);
  }
}

bool Radio::SetMonitoringState() {
  uint8_t request[5] = {0, 0, 0, static_cast<uint8_t>(
      (global_metadata_monitoring_enabled_ << 3)),
      0,
  };
  uint8_t response[2];
  bool success = SendCommand(
      Transport::OpCode::SetFeatureMonitorRequest,
      Transport::OpCode::SetFeatureMonitorResponse,
      request, sizeof(request), response, sizeof(response), 100ms);
  if (success) {
    auto status = UnpackStatus(response);
    success = (status == Status::Success);
    if (!success) {
      LOGE("Set monitoring state failed with 0x%04", PRIx16, status);
    }
  }

  return success;
}

bool Radio::ParseMetadata(const uint8_t *payload, size_t size,
                          Metadata *data) {
  bool success = (size >= 2);
  if (!success) {
    LOGE("Short metadata packet");
  } else {
    uint8_t field_count = payload[0];
    size_t parsing_offset = 1;
    for (uint8_t i = 0; i < field_count; i++) {
      if ((parsing_offset + 1) >= size) {
        LOGE("Short metadata packet");
        success = false;
        break;
      }

      uint8_t str_type = payload[parsing_offset++];
      uint8_t length = payload[parsing_offset++];
      if ((parsing_offset + length - 1) >= size) {
        LOGE("Short metadata packet");
        success = false;
        break;
      }

      auto *str = reinterpret_cast<const char *>(&payload[parsing_offset]);
      PopulateMetadataEventField(data, str_type, std::string(str, length));
      parsing_offset += length;
    }
  }

  return success;
}

void Radio::HandleMetadataPacket(const uint8_t *payload, size_t size) {
  if (size < 2) {
    LOGE("Short metadata packet");
  } else {
    Metadata data;
    uint8_t channel_id = payload[0];
    if (ParseMetadata(&payload[1], size - 1, &data)) {
      event_handler_->OnMetadataChange(channel_id, data);
    }
  }
}

void Radio::PopulateMetadataEventField(
    Metadata *data, uint8_t str_type, std::string str) {
  switch (static_cast<MetadataType>(str_type)) {
    case MetadataType::Artist:
      data->artist = str;
      break;
    case MetadataType::Title:
      data->title = str;
      break;
    case MetadataType::Album:
      data->album = str;
      break;
    case MetadataType::RecordLabel:
      data->record_label = str;
      break;
    case MetadataType::Composer:
      data->composer = str;
      break;
    case MetadataType::AltArtist:
      data->alt_artist = str;
      break;
    case MetadataType::Comments:
      data->comments = str;
      break;
    case MetadataType::PromoText1:
    case MetadataType::PromoText2:
    case MetadataType::PromoText3:
    case MetadataType::PromoText4:
      data->promo_text.push_back(str);
      break;
    case MetadataType::SongId:
    case MetadataType::ArtistId:
    case MetadataType::Empty:
      // Ignore these for now. They are not printable strings.
      break;
    default:
      LOGE("Unsupported metadata 0x%02" PRIx8, str_type);
      break;
  }
}

bool Radio::SendCommand(Transport::OpCode request_op_code,
                        Transport::OpCode response_op_code,
                        const uint8_t *command, size_t command_size,
                        uint8_t *response, size_t response_size,
                        std::chrono::milliseconds timeout) {
  std::unique_lock<std::mutex> lock(mutex_);
  response_op_code_ = response_op_code;
  response_ = response;
  response_size_ = response_size;

  transport_.SendMessageFrame(request_op_code, command, command_size);
  bool timed_out = (cv_.wait_for(lock, timeout) == std::cv_status::timeout);
  if (timed_out) {
    LOGE("Request 0x%04" PRIx16 " timed out", request_op_code);
  }

  return !timed_out;
}

bool Radio::WaitPut(Transport::OpCode put_op_code,
                    uint8_t *put, size_t put_size,
                    std::chrono::milliseconds timeout) {
  std::unique_lock<std::mutex> lock(mutex_);
  response_op_code_ = put_op_code;
  response_ = put;
  response_size_ = put_size;

  bool timed_out = (cv_.wait_for(lock, timeout) == std::cv_status::timeout);
  if (timed_out) {
    LOGE("Put 0x%04" PRIx16 " timed out", put_op_code);
  }

  return !timed_out;
}

}  // namespace dogtricks
