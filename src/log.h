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

#ifndef DOGTRICKS_LOG_H_
#define DOGTRICKS_LOG_H_

#include <cstdio>
#include <cstdlib>

// TODO: fprintf is not thread safe, printing there from many threads is
// race-ey. Should probably have some locks.

#define LOG_FUNC(fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)

#define LOGD(fmt, ...) LOG_FUNC(fmt, ##__VA_ARGS__)
#define LOGI(fmt, ...) LOG_FUNC(fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) LOG_FUNC(fmt, ##__VA_ARGS__)

#define FATAL_ERROR(fmt, ...) do { \
    LOGE(fmt, ##__VA_ARGS__); \
    abort(); \
  } while (0);

#endif  // DOGTRICKS_LOG_H_
