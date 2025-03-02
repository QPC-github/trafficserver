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


add_library(http STATIC
        HttpSessionAccept.cc
        HttpBodyFactory.cc
        HttpCacheSM.cc
        Http1ClientSession.cc
        Http1ClientTransaction.cc
        Http1ServerTransaction.cc
        HttpConfig.cc
        HttpConnectionCount.cc
        HttpDebugNames.cc
        HttpPages.cc
        HttpProxyServerMain.cc
        HttpSM.cc
        Http1ServerSession.cc
        HttpSessionManager.cc
        HttpTransact.cc
        HttpTransactCache.cc
        HttpTransactHeaders.cc
        HttpTunnel.cc
        ForwardedConfig.cc
        PreWarmConfig.cc
        PreWarmManager.cc
)
target_include_directories(http PRIVATE
        ${IOCORE_INCLUDE_DIRS}
        ${PROXY_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/mgmt
        ${CMAKE_SOURCE_DIR}/mgmt/utils
        ${YAMLCPP_INCLUDE_DIR}
)
add_subdirectory(remap)