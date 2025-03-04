#  Licensed to the Apache Software Foundation (ASF) under one
#  or more contributor license agreements.  See the NOTICE file
#  distributed with this work for additional information
#  regarding copyright ownership.  The ASF licenses this file
#  to you under the Apache License, Version 2.0 (the
#  "License"); you may not use this file except in compliance
#  with the License.  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

pkglib_LTLIBRARIES += authproxy/authproxy.la
authproxy_authproxy_la_SOURCES = \
	authproxy/authproxy.cc \
	authproxy/utils.cc \
	authproxy/utils.h

check_PROGRAMS +=  authproxy/authproxy_test
authproxy_authproxy_test_CPPFLAGS = $(AM_CPPFLAGS) -I$(abs_top_srcdir)/tests/include
authproxy_authproxy_test_SOURCES = authproxy/tests/authproxy_test.cc
