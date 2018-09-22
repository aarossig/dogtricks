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

int main(int argc, char **argv) {
  TCLAP::CmdLine cmd(kDescription, ' ', kVersion);
  TCLAP::ValueArg<std::string> path_arg("", "path",
      "the path of the serial device to communicate with",
      false /* req */, "/dev/ttyUSB0", "path", cmd);
  TCLAP::SwitchArg reset_arg("", "reset",
      "reset the radio before executing other commands", cmd);
  TCLAP::SwitchArg log_signal_strength_arg("", "log_signal_strength",
      "logs the current signal strength", cmd);
  TCLAP::ValueArg<int> set_channel_arg("", "set_channel",
      "sets the channel that the radio is decoding",
      false /* req */, 51 /* eurobeat intensifies */, "channel", cmd);
  cmd.parse(argc, argv);

  Radio radio(path_arg.getValue().c_str());
  std::thread receive_thread([&radio](){
    if (!radio.Start()) {
      LOGE("Failed to start receive loop for radio");
    }
  });

  gRadioInstance = &radio;
  std::signal(SIGINT, SignalHandler);

  bool success = radio.IsOpen();
  if (success && reset_arg.isSet()) {
    success &= radio.Reset();
  }

  // Ensure that the radio is in full power mode.
  radio.SetPowerMode(Radio::PowerState::FullMode);

  if (success && log_signal_strength_arg.isSet()) {
    success &= radio.GetSignalStrength();
  }

  if (success && set_channel_arg.isSet()) {
    success &= radio.SetChannel(set_channel_arg.getValue());
  }

  radio.Stop();
  receive_thread.join();

  return (success ? 0 : -1);
}
