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


add_library(logging STATIC
        Log.cc
        LogAccess.cc
        LogBuffer.cc
        LogConfig.cc
        LogField.cc
        LogFieldAliasMap.cc
        LogFile.cc
        LogFilter.cc
        LogFormat.cc
        LogObject.cc
        LogUtils.cc
        RolledLogDeleter.cc
        YamlLogConfig.cc
        YamlLogConfigDecoders.cc
)
target_include_directories(logging PRIVATE
        ${IOCORE_INCLUDE_DIRS}
        ${PROXY_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/mgmt
        ${CMAKE_SOURCE_DIR}/mgmt/utils
        ${YAML_INCLUDE_DIRS}
        ${SWOC_INCLUDE_DIR}
)