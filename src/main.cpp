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

#include <cinttypes>
#include <cstdio>
#include <csignal>
#include <string>
#include <tclap/CmdLine.h>
#include <thread>

#include "log.h"
#include "radio.h"

using dogtricks::Radio;

//! A description of the program.
constexpr char kDescription[] = "A tool for making satellite radio dogs do tricks.";

//! The version of the program.
constexpr char kVersion[] = "0.0.1";

//! The radio instance that will be stopped when SIGINT is raised.
Radio *gRadioInstance = nullptr;

/**
 * Handle signals to stop the radio receive loop gracefully.
 */
void SignalHandler(int signal) {
  if (gRadioInstance != nullptr) {
    LOGD("Stopping");
    gRadioInstance->Stop();
  }
}

/**
 * Logs the supplied event with a two-space indent.
 *
 * @param event The event to log.
 */
void LogMetadata(const Radio::Metadata& event) {
  if (event.artist.has_value()) {
    LOGD("  artist: %s", event.artist.value().c_str());
  }

  if (event.title.has_value()) {
    LOGD("  title: %s", event.title.value().c_str());
  }

  if (event.album.has_value()) {
    LOGD("  album: %s", event.album.value().c_str());
  }

  if (event.record_label.has_value()) {
    LOGD("  record label: %s", event.record_label.value().c_str());
  }

  if (event.composer.has_value()) {
    LOGD("  composer: %s", event.composer.value().c_str());
  }

  if (event.alt_artist.has_value()) {
    LOGD("  alt artist: %s", event.alt_artist.value().c_str());
  }

  if (event.comments.has_value()) {
    LOGD("  comments: %s", event.comments.value().c_str());
  }

  for (size_t i = 0; i < event.promo_text.size(); i++) {
    LOGD("  promo %zu: %s", i, event.promo_text[i].c_str());
  }
}

/**
 * Logs the supplied channel descriptor.
 *
 * @param desc The descriptor to log.
 */
void LogChannelDescriptor(const Radio::ChannelDescriptor& desc) {
  LOGI("Channel %" PRId8 ":", desc.channel_id);
  LOGI("  category id: %" PRId8 ":", desc.category_id);
  LOGI("  short name: %s", desc.short_name.c_str());
  LOGI("  long name: %s", desc.long_name.c_str());
  LOGI("  short category name: %s", desc.short_category_name.c_str());
  LOGI("  long category name: %s", desc.long_category_name.c_str());
  LogMetadata(desc.metadata);
}

void LogSignalStrength(Radio::SignalStrength summary,
                       Radio::SignalStrength satellite,
                       Radio::SignalStrength terrestrial) {
  LOGI("Signal strength:");
  LOGI("  summary: %s", Radio::GetSignalDescription(summary));
  LOGI("  satellite: %s", Radio::GetSignalDescription(satellite));
  LOGI("  terrestrial: %s", Radio::GetSignalDescription(terrestrial));
}

/**
 * An implementation of the radio event handler for the command line tool.
 */
class RadioEventHandler : public Radio::EventHandler {
 public:
  virtual void OnMetadataChange(uint8_t channel_id,
                                const Radio::Metadata& event) override {
    LOGD("Metadata changed:");
    LOGD("  channel_id: %" PRId8, channel_id);
    LogMetadata(event);
  }
};

int main(int argc, char **argv) {
  TCLAP::CmdLine cmd(kDescription, ' ', kVersion);
  TCLAP::ValueArg<std::string> path_arg("", "path",
      "the path of the serial device to communicate with",
      false /* req */, "/dev/ttyUSB0", "path", cmd);
  TCLAP::SwitchArg reset_arg("", "reset",
      "reset the radio before executing other commands", cmd);
  TCLAP::SwitchArg log_signal_strength_arg("", "log_signal_strength",
      "logs the current signal strength", cmd);
  TCLAP::SwitchArg log_global_metadata_arg("", "log_global_metadata",
      "logs all changes in channel metadata", cmd);
  TCLAP::SwitchArg list_channels_arg("", "list_channels",
      "logs the list of channels available", cmd);
  TCLAP::ValueArg<int> get_channel_arg("", "get_channel",
      "gets the channel descriptor and logs it",
      false /* req */, 51 /* unce unce unce */, "channel", cmd);
  TCLAP::ValueArg<int> set_channel_arg("", "set_channel",
      "sets the channel that the radio is decoding",
      false /* req */, 51 /* eurobeat intensifies */, "channel", cmd);
  cmd.parse(argc, argv);

  RadioEventHandler event_handler;
  Radio radio(path_arg.getValue().c_str(), &event_handler);
  std::thread receive_thread([&radio](){
    if (!radio.Start()) {
      LOGE("Failed to start receive loop for radio");
    }
  });

  gRadioInstance = &radio;
  std::signal(SIGINT, SignalHandler);

  bool quit = true;  // Set to true when the program should quit.
  bool success = radio.IsOpen();
  if (success && reset_arg.isSet()) {
    success &= radio.Reset();
  }

  // Ensure that the radio is in full power mode.
  radio.SetPowerMode(Radio::PowerState::FullMode);

  if (success && log_signal_strength_arg.isSet()) {
    Radio::SignalStrength summary;
    Radio::SignalStrength satellite;
    Radio::SignalStrength terrestrial;
    success &= radio.GetSignalStrength(&summary, &satellite, &terrestrial);
    if (success) {
      LogSignalStrength(summary, satellite, terrestrial);
    } 
  }

  if (success && list_channels_arg.isSet()) {
    Radio::ChannelList channels;
    success &= radio.GetChannelList(&channels);
    for (uint8_t channel : channels) {
      Radio::ChannelDescriptor desc;
      success &= radio.GetChannelDescriptor(channel, &desc);
      if (success) {
        LogChannelDescriptor(desc);
      }
    }
  }

  if (success && log_global_metadata_arg.isSet()) {
    success &= radio.SetGlobalMetadataMonitoringEnabled(true);
    quit = false;
  }

  if (success && get_channel_arg.isSet()) {
    Radio::ChannelDescriptor desc;
    success &= radio.GetChannelDescriptor(get_channel_arg.getValue(), &desc);
    if (success) {
      LogChannelDescriptor(desc);
    }
  }

  if (success && set_channel_arg.isSet()) {
    success &= radio.SetChannel(set_channel_arg.getValue());
  }

  if (quit) {
    radio.Stop();
  }

  receive_thread.join();

  return (success ? 0 : -1);
}
