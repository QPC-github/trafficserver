#######################
#
#  Licensed to the Apache Software Foundation (ASF) under one or more contributor license
#  agreements.  See the NOTICE file distributed with this work for additional information regarding
#  copyright ownership.  The ASF licenses this file to you under the Apache License, Version 2.0
#  (the "License"); you may not use this file except in compliance with the License.  You may obtain
#  a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software distributed under the License
#  is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
#  or implied. See the License for the specific language governing permissions and limitations under
#  the License.
#
#######################


add_library(records_lm STATIC
        P_RecCore.cc
        RecConfigParse.cc
        RecCore.cc
        RecDebug.cc
        RecFile.cc
        RecHttp.cc
        RecMessage.cc
        RecMutex.cc
        RecRawStats.cc
        RecUtils.cc
        RecYAMLDecoder.cc
        RecordsConfig.cc
        RecordsConfigUtils.cc
)
target_include_directories(records_lm PRIVATE
        ${CMAKE_SOURCE_DIR}/mgmt
        ${CMAKE_SOURCE_DIR}/mgmt/api/include
        ${CMAKE_SOURCE_DIR}/mgmt/utils
        ${CMAKE_SOURCE_DIR}/iocore/eventsystem
        ${CMAKE_SOURCE_DIR}/iocore/utils
)

add_library(records_p STATIC
        P_RecCore.cc
        RecConfigParse.cc
        RecCore.cc
        RecDebug.cc
        RecFile.cc
        RecHttp.cc
        RecMessage.cc
        RecMutex.cc
        RecRawStats.cc
        RecUtils.cc
        RecProcess.cc
        RecYAMLDecoder.cc
        RecordsConfig.cc
        RecordsConfigUtils.cc
        )
target_include_directories(records_p PRIVATE
        ${CMAKE_SOURCE_DIR}/mgmt
        ${CMAKE_SOURCE_DIR}/mgmt/api/include
        ${CMAKE_SOURCE_DIR}/mgmt/utils
        ${CMAKE_SOURCE_DIR}/iocore/eventsystem
        ${CMAKE_SOURCE_DIR}/iocore/utils
        )
