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

namespace dogtricks {

bool Radio::Start() {
  return transport_.Start();
}

void Radio::Stop() {
  transport_.Stop();
}

bool Radio::Reset() {
  uint8_t response[2];
  bool success = SendCommand(
      Transport::OpCode::SetResetRequest,
      Transport::OpCode::SetResetResponse,
      nullptr, 0, response, sizeof(response), 100ms);
  if (success) {
    Transport::Status status = Transport::UnpackStatus(response);
    success = (status == Transport::Status::Success);
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
    auto status = Transport::UnpackStatus(response);
    success = (status == Transport::Status::Success);
    if (!success) {
      LOGE("Set channel request failed with 0x%04" PRIx16, status);
    }
  }

  return success;
}

bool Radio::GetSignalStrength() {
  uint8_t response[6];
  bool success = SendCommand(
      Transport::OpCode::GetSignalRequest,
      Transport::OpCode::GetSignalResponse,
      nullptr, 0, response, sizeof(response), 100ms);
  if (success) {
    auto status = Transport::UnpackStatus(response);
    success = (status == Transport::Status::Success);
    if (!success) {
      LOGE("Get signal strength request failed with 0x%04" PRIx16, status);
    } else {
      LOGI("Signal strength:");
      LOGI("  summary: %s", Transport::GetSignalDescription(response[2]));
      LOGI("  satellite: %s", Transport::GetSignalDescription(response[3]));
      LOGI("  terrestrial: %s", Transport::GetSignalDescription(response[4]));
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
    auto status = Transport::UnpackStatus(response);
    success = (status == Transport::Status::Success);
    if (!success) {
      LOGE("Get channel list request failed with 0x%04", PRIx16, status);
    } else {
      uint8_t channel_count = response[0];
      channels->resize(channel_count);
      for (uint8_t i = 0; i < channel_count; i++) {
        channels->push_back(response[1 + i]);
      }
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
    auto status = Transport::UnpackStatus(response);
    success = (status == Transport::Status::Success);
    if (!success) {
      LOGE("Set monitoring state failed with 0x%04", PRIx16, status);
    }
  }

  return success;
}

void Radio::HandleMetadataPacket(const uint8_t *payload, size_t size) {
  if (size < 2) {
    LOGE("Short metadata packet");
  } else {
    MetadataEvent event;
    event.channel_id = payload[0];

    bool success = true;
    uint8_t field_count = payload[1];
    size_t parsing_offset = 2;
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
      PopulateMetadataEventField(&event, str_type, std::string(str, length));
      parsing_offset += length;
    }

    if (success) {
      event_handler_->OnMetadataChange(event);
    }
  }
}

void Radio::PopulateMetadataEventField(
    MetadataEvent *event, uint8_t str_type, std::string str) {
  switch (static_cast<Transport::MetadataType>(str_type)) {
    case Transport::MetadataType::Artist:
      event->artist = str;
      break;
    case Transport::MetadataType::Title:
      event->title = str;
      break;
    case Transport::MetadataType::Album:
      event->album = str;
      break;
    case Transport::MetadataType::RecordLabel:
      event->record_label = str;
      break;
    case Transport::MetadataType::Composer:
      event->composer = str;
      break;
    case Transport::MetadataType::AltArtist:
      event->alt_artist = str;
      break;
    case Transport::MetadataType::Comments:
      event->comments = str;
      break;
    case Transport::MetadataType::PromoText1:
    case Transport::MetadataType::PromoText2:
    case Transport::MetadataType::PromoText3:
    case Transport::MetadataType::PromoText4:
      event->promo_text.push_back(str);
      break;
    case Transport::MetadataType::SongId:
    case Transport::MetadataType::ArtistId:
    case Transport::MetadataType::Empty:
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
