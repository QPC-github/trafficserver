#
# Makefile.am for the Enterprise Management module.
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

bin_PROGRAMS += traffic_server/traffic_server

traffic_server_traffic_server_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	$(iocore_include_dirs) \
	-I$(abs_top_srcdir)/include \
	-I$(abs_top_srcdir)/lib \
	-I$(abs_top_srcdir)/proxy \
	-I$(abs_top_srcdir)/proxy/http \
	-I$(abs_top_srcdir)/proxy/http2 \
	-I$(abs_top_srcdir)/proxy/http3 \
	-I$(abs_top_srcdir)/proxy/logging \
	-I$(abs_top_srcdir)/proxy/http/remap \
	-I$(abs_top_srcdir)/proxy/hdrs \
	-I$(abs_top_srcdir)/proxy/shared \
	-I$(abs_top_srcdir)/mgmt \
	-I$(abs_top_srcdir)/mgmt/utils \
	@SWOC_INCLUDES@ \
	$(TS_INCLUDES) \
	@OPENSSL_INCLUDES@ \
	@BORINGOCSP_INCLUDES@ \
	@YAMLCPP_INCLUDES@

traffic_server_traffic_server_LDFLAGS = \
	$(AM_LDFLAGS) \
        @SWOC_LDFLAGS@ \
	@YAMLCPP_LDFLAGS@ \
	@BORINGOCSP_LDFLAGS@

traffic_server_traffic_server_SOURCES = \
	traffic_server/Crash.cc \
	traffic_server/EventName.cc \
	traffic_server/EventName.h \
	traffic_server/FetchSM.cc \
	traffic_server/FetchSM.h \
	traffic_server/HostStatus.cc \
	traffic_server/InkAPI.cc \
	traffic_server/InkIOCoreAPI.cc \
	traffic_server/SocksProxy.cc \
	traffic_server/RpcAdminPubHandlers.cc \
	traffic_server/RpcAdminPubHandlers.h \
	shared/overridable_txn_vars.cc \
	traffic_server/traffic_server.cc


if BUILD_TESTS
traffic_server_traffic_server_SOURCES += \
	traffic_server/InkAPITest.cc
endif

traffic_server_traffic_server_LDADD = \
	$(top_builddir)/proxy/http/libhttp.a \
	$(top_builddir)/proxy/http/remap/libhttp_remap.a \
	$(top_builddir)/proxy/http2/libhttp2.a \
	$(top_builddir)/proxy/logging/liblogging.a \
	$(top_builddir)/proxy/hdrs/libhdrs.a \
	$(top_builddir)/proxy/shared/libdiagsconfig.a \
	$(top_builddir)/iocore/utils/libinkutils.a \
	$(top_builddir)/iocore/dns/libinkdns.a \
	$(top_builddir)/iocore/hostdb/libinkhostdb.a \
	$(top_builddir)/iocore/dns/libinkdns.a \
	$(top_builddir)/iocore/cache/libinkcache.a \
	$(top_builddir)/lib/fastlz/libfastlz.a \
	$(top_builddir)/iocore/aio/libinkaio.a \
	$(top_builddir)/src/tscore/libtscore.la \
	$(top_builddir)/src/tscpp/util/libtscpputil.la \
	$(top_builddir)/proxy/libproxy.a \
	$(top_builddir)/iocore/net/libinknet.a \
	$(top_builddir)/src/records/librecords_p.a \
	$(top_builddir)/iocore/eventsystem/libinkevent.a \
	$(top_builddir)/mgmt/rpc/libjsonrpc_server.la \
	$(top_builddir)/mgmt/rpc/libjsonrpc_protocol.la \
	$(top_builddir)/mgmt/config/libconfigmanager.la \
	$(top_builddir)/mgmt/rpc/librpcpublichandlers.la \
	$(top_builddir)/mgmt/utils/libutils_p.la \
	@HWLOC_LIBS@ \
	@LIBPCRE@ \
	@LIBRESOLV@ \
	@LIBZ@ \
	@LIBLZMA@ \
	@LIBPROFILER@ \
	@SWOC_LIBS@ \
	@OPENSSL_LIBS@ \
	@YAMLCPP_LIBS@ \
	@BORINGOCSP_LIBS@ \
	@URING_LIBS@ \
	-lm

if IS_DARWIN
if !IS_DARWIN_ARM64
traffic_server_traffic_server_LDADD += $(LUAJIT_DARWIN_LDFLAGS)
endif
endif

if ENABLE_QUIC
traffic_server_traffic_server_LDADD += \
  $(top_builddir)/proxy/http3/libhttp3.a \
  $(top_builddir)/iocore/net/quic/libquic.a \
  $(top_builddir)/iocore/eventsystem/libinkevent.a
if USE_QUICHE
traffic_server_traffic_server_LDADD += \
  $(QUICHE_LIB)
endif
endif

if ENABLE_IO_URING
traffic_server_traffic_server_LDADD += \
  $(top_builddir)/iocore/io_uring/libinkuring.a
endif
