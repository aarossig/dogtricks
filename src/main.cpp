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
#include <tclap/CmdLine.h>

//! A description of the program.
constexpr char kDescription[] = "A tool for making satellite radio dogs do tricks.";

//! The version of the program.
constexpr char kVersion[] = "0.0.1";

int main(int argc, char **argv) {
  TCLAP::CmdLine cmd(kDescription, ' ', kVersion);
  cmd.parse(argc, argv);

  return 0;
}
