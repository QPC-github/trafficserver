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



# yamlcpp shadows a bunch of variables but we don't want to hear bout it
add_compile_options(-Wno-shadow)
set(BUILD_SHARED_LIBS 1)
add_subdirectory(yamlcpp)
set(YAML_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/lib/yamlcpp/include PARENT_SCOPE)
add_subdirectory(fastlz)
add_subdirectory(swoc)
set(SWOC_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/lib/swoc/include PARENT_SCOPE)
