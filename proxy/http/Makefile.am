# Makefile.am for Http
#
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

include $(top_srcdir)/build/tidy.mk

SUBDIRS = remap

AM_CPPFLAGS += \
	$(iocore_include_dirs) \
	-I$(abs_top_srcdir)/proxy \
	-I$(abs_top_srcdir)/include \
	-I$(abs_top_srcdir)/lib \
	-I$(abs_top_srcdir)/mgmt \
	-I$(abs_top_srcdir)/mgmt/utils \
	-I$(abs_top_srcdir)/proxy/hdrs \
	-I$(abs_top_srcdir)/proxy/shared \
	-I$(abs_top_srcdir)/proxy/http/remap \
	-I$(abs_top_srcdir)/proxy/logging \
	-I$(abs_top_srcdir)/proxy/http2 \
	-I$(abs_top_srcdir)/proxy/http3 \
	@SWOC_INCLUDES@ \
	$(TS_INCLUDES) \
	@YAMLCPP_INCLUDES@

noinst_HEADERS = HttpProxyServerMain.h
noinst_LIBRARIES = libhttp.a

libhttp_a_SOURCES = \
	HttpSessionAccept.cc \
	HttpSessionAccept.h \
	HttpBodyFactory.cc \
	HttpBodyFactory.h \
	HttpCacheSM.cc \
	HttpCacheSM.h \
	Http1ClientSession.cc \
	Http1ClientSession.h \
	Http1ClientTransaction.cc \
	Http1Transaction.h \
	Http1ClientTransaction.h \
	Http1ServerTransaction.h \
	Http1ServerTransaction.cc \
	HttpConfig.cc \
	HttpConfig.h \
	HttpConnectionCount.cc \
	HttpConnectionCount.h \
	HttpDebugNames.cc \
	HttpDebugNames.h \
	HttpPages.cc \
	HttpPages.h \
	HttpProxyServerMain.cc \
	HttpProxyServerMain.h \
	HttpSM.cc \
	HttpSM.h \
	Http1ServerSession.cc \
	Http1ServerSession.h \
	HttpSessionManager.cc \
	HttpSessionManager.h \
	HttpTransact.cc \
	HttpTransact.h \
	HttpTransactCache.cc \
	HttpTransactCache.h \
	HttpTransactHeaders.cc \
	HttpTransactHeaders.h \
	HttpTunnel.cc \
	HttpTunnel.h \
	ForwardedConfig.cc \
	PreWarmConfig.cc \
	PreWarmManager.cc

if BUILD_TESTS
libhttp_a_SOURCES += RegressionHttpTransact.cc
endif

check_PROGRAMS = test_proxy_http test_PreWarm

TESTS = $(check_PROGRAMS)

test_proxy_http_CPPFLAGS = $(AM_CPPFLAGS)\
	-I$(abs_top_srcdir)/tests/include

test_proxy_http_SOURCES = \
	unit_tests/unit_test_main.cc \
	unit_tests/test_ForwardedConfig.cc \
	ForwardedConfig.cc \
	unit_tests/test_error_page_selection.cc \
	HttpBodyFactory.cc \
	HttpBodyFactory.h

test_proxy_http_LDADD = \
	$(top_builddir)/src/tscpp/util/libtscpputil.la \
	$(top_builddir)/src/tscore/libtscore.la \
	$(top_builddir)/proxy/hdrs/libhdrs.a \
	$(top_builddir)/proxy/logging/liblogging.a \
	$(top_builddir)/iocore/eventsystem/libinkevent.a \
	$(top_builddir)/src/records/librecords_p.a \
	$(top_builddir)/iocore/utils/libinkutils.a \
	@SWOC_LIBS@ @HWLOC_LIBS@ \
	@LIBCAP@

test_PreWarm_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(abs_top_srcdir)/tests/include

test_PreWarm_LDADD = \
	$(top_builddir)/src/tscore/libtscore.la

test_PreWarm_SOURCES = \
	unit_tests/test_PreWarm.cc

clang-tidy-local: $(libhttp_a_SOURCES) $(noinst_HEADERS)
	$(CXX_Clang_Tidy)

#test_UNUSED_SOURCES = \
#  TestHttpTransact.cc \
#  TestUrl.cc \
#  test_socket_close.cc \
#  testheaders.cc
