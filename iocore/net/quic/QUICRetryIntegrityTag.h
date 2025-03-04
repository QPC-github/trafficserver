/** @file
 *
 *  A brief file description
 *
 *  @section license License
 *
 *  Licensed to the Apache Software Foundation (ASF) under one
 *  or more contributor license agreements.  See the NOTICE file
 *  distributed with this work for additional information
 *  regarding copyright ownership.  The ASF licenses this file
 *  to you under the Apache License, Version 2.0 (the
 *  "License"); you may not use this file except in compliance
 *  with the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#pragma once

#include "QUICTypes.h"

class QUICRetryIntegrityTag
{
public:
  static constexpr int LEN = 16;
  static bool compute(uint8_t *out, QUICVersion version, QUICConnectionId odcid, Ptr<IOBufferBlock> header,
                      Ptr<IOBufferBlock> payload);

private:
  // For version 1
  static constexpr uint8_t KEY_FOR_RETRY_INTEGRITY_TAG[]   = {0xbe, 0x0c, 0x69, 0x0b, 0x9f, 0x66, 0x57, 0x5a,
                                                              0x1d, 0x76, 0x6b, 0x54, 0xe3, 0x68, 0xc8, 0x4e};
  static constexpr uint8_t NONCE_FOR_RETRY_INTEGRITY_TAG[] = {0x46, 0x15, 0x99, 0xd3, 0x5d, 0x63,
                                                              0x2b, 0xf2, 0x23, 0x98, 0x25, 0xbb};
  // For draft 29
  static constexpr uint8_t KEY_FOR_RETRY_INTEGRITY_TAG_D29[]   = {0xcc, 0xce, 0x18, 0x7e, 0xd0, 0x9a, 0x09, 0xd0,
                                                                  0x57, 0x28, 0x15, 0x5a, 0x6c, 0xb9, 0x6b, 0xe1};
  static constexpr uint8_t NONCE_FOR_RETRY_INTEGRITY_TAG_D29[] = {0xe5, 0x49, 0x30, 0xf9, 0x7f, 0x21,
                                                                  0x36, 0xf0, 0x53, 0x0a, 0x8c, 0x1c};
};
