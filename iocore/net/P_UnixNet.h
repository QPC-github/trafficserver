/** @file

  A brief file description

  @section license License

  Licensed to the Apache Software Foundation (ASF) under one
  or more contributor license agreements.  See the NOTICE file
  distributed with this work for additional information
  regarding copyright ownership.  The ASF licenses this file
  to you under the Apache License, Version 2.0 (the
  "License"); you may not use this file except in compliance
  with the License.  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
 */

#pragma once

#include <bitset>

#include "tscore/ink_platform.h"

#define USE_EDGE_TRIGGER_EPOLL  1
#define USE_EDGE_TRIGGER_KQUEUE 1
#define USE_EDGE_TRIGGER_PORT   1

#define EVENTIO_NETACCEPT      1
#define EVENTIO_READWRITE_VC   2
#define EVENTIO_DNS_CONNECTION 3
#define EVENTIO_UDP_CONNECTION 4
#define EVENTIO_ASYNC_SIGNAL   5
#define EVENTIO_IO_URING       6

#if TS_USE_EPOLL
#ifndef EPOLLEXCLUSIVE
#define EPOLLEXCLUSIVE 0
#endif
#ifdef USE_EDGE_TRIGGER_EPOLL
#define USE_EDGE_TRIGGER 1
#define EVENTIO_READ     (EPOLLIN | EPOLLET)
#define EVENTIO_WRITE    (EPOLLOUT | EPOLLET)
#else
#define EVENTIO_READ  EPOLLIN
#define EVENTIO_WRITE EPOLLOUT
#endif
#define EVENTIO_ERROR (EPOLLERR | EPOLLPRI | EPOLLHUP)
#endif

#if TS_USE_KQUEUE
#ifdef USE_EDGE_TRIGGER_KQUEUE
#define USE_EDGE_TRIGGER    1
#define INK_EV_EDGE_TRIGGER EV_CLEAR
#else
#define INK_EV_EDGE_TRIGGER 0
#endif
#define EVENTIO_READ  INK_EVP_IN
#define EVENTIO_WRITE INK_EVP_OUT
#define EVENTIO_ERROR (0x010 | 0x002 | 0x020) // ERR PRI HUP
#endif

struct PollDescriptor;
typedef PollDescriptor *EventLoop;

class NetEvent;
class UnixUDPConnection;
class DiskHandler;
struct DNSConnection;
struct NetAccept;

/// Unified API for setting and clearing kernel and epoll events.
struct EventIO {
  int fd = -1; ///< file descriptor, often a system port
#if TS_USE_KQUEUE || TS_USE_EPOLL && !defined(USE_EDGE_TRIGGER)
  int events = 0; ///< a bit mask of enabled events
#endif
  EventLoop event_loop = nullptr; ///< the assigned event loop
  bool syscall         = true;    ///< if false, disable all functionality (for QUIC)
  int type             = 0;       ///< class identifier of union data.
  union {
    void *untyped;
    NetEvent *ne;
    DNSConnection *dnscon;
    NetAccept *na;
    UnixUDPConnection *uc;
    DiskHandler *dh;
  } data; ///< a kind of continuation

  /** The start methods all logically Setup a class to be called
     when a file descriptor is available for read or write.
     The type of the classes vary.  Generally the file descriptor
     is pulled from the class, but there is one option that lets
     the file descriptor be expressed directly.
     @param l the event loop
     @param events a mask of flags (for details `man epoll_ctl`)
     @return int the number of events created, -1 is error
   */
  int start(EventLoop l, DNSConnection *vc, int events);
  int start(EventLoop l, NetAccept *vc, int events);
  int start(EventLoop l, NetEvent *ne, int events);
  int start(EventLoop l, UnixUDPConnection *vc, int events);
  int start(EventLoop l, int fd, NetEvent *ne, int events);
  int start_common(EventLoop l, int fd, int events);

  /** Alter the events that will trigger the continuation, for level triggered I/O.
     @param events add with positive mask(+EVENTIO_READ), or remove with negative mask (-EVENTIO_READ)
     @return int the number of events created, -1 is error
   */
  int modify(int events);

  /** Refresh the existing events (i.e. KQUEUE EV_CLEAR), for edge triggered I/O
     @param events mask of events
     @return int the number of events created, -1 is error
   */
  int refresh(int events);

  /// Remove the kernel or epoll event. Returns 0 on success.
  int stop();

  /// Remove the epoll event and close the connection. Returns 0 on success.
  int close();

  EventIO() { data.untyped = nullptr; }
};

#include "P_Net.h"
#include "P_UnixNetProcessor.h"
#include "P_UnixNetVConnection.h"
#include "P_NetAccept.h"
#include "P_DNSConnection.h"
#include "P_UnixUDPConnection.h"
#include "P_UnixPollDescriptor.h"
#include <limits>

class NetEvent;
class NetHandler;
typedef int (NetHandler::*NetContHandler)(int, void *);
typedef unsigned int uint32;

extern ink_hrtime last_throttle_warning;
extern ink_hrtime last_shedding_warning;
extern ink_hrtime emergency_throttle_time;
extern int net_connections_throttle;
extern bool net_memory_throttle;
extern int fds_throttle;
extern int fds_limit;
extern ink_hrtime last_transient_accept_error;
extern int http_accept_port_number;

//
// Configuration Parameter had to move here to share
// between UnixNet and UnixUDPNet or SSLNet modules.
// Design notes are in Memo.NetDesign
//

#define THROTTLE_FD_HEADROOM (128 + 64) // CACHE_DB_FDS + 64

#define TRANSIENT_ACCEPT_ERROR_MESSAGE_EVERY HRTIME_HOURS(24)

// also the 'throttle connect headroom'
#define EMERGENCY_THROTTLE       16
#define THROTTLE_AT_ONCE         5
#define HYPER_EMERGENCY_THROTTLE 6

#define NET_THROTTLE_ACCEPT_HEADROOM  1.1 // 10%
#define NET_THROTTLE_CONNECT_HEADROOM 1.0 // 0%
#define NET_THROTTLE_MESSAGE_EVERY    HRTIME_MINUTES(10)

#define PRINT_IP(x) ((uint8_t *)&(x))[0], ((uint8_t *)&(x))[1], ((uint8_t *)&(x))[2], ((uint8_t *)&(x))[3]

// function prototype needed for SSLUnixNetVConnection
unsigned int net_next_connection_number();

struct PollCont : public Continuation {
  NetHandler *net_handler;
  PollDescriptor *pollDescriptor;
  PollDescriptor *nextPollDescriptor;
  int poll_timeout;

  PollCont(Ptr<ProxyMutex> &m, int pt = net_config_poll_timeout);
  PollCont(Ptr<ProxyMutex> &m, NetHandler *nh, int pt = net_config_poll_timeout);
  ~PollCont() override;
  int pollEvent(int, Event *);
  void do_poll(ink_hrtime timeout);
};

/**
  NetHandler is the processor of NetEvent for the Net sub-system. The NetHandler
  is the core component of the Net sub-system. Once started, it is responsible
  for polling socket fds and perform the I/O tasks in NetEvent.

  The NetHandler is executed periodically to perform read/write tasks for
  NetVConnection. The NetHandler::mainNetEvent() should be viewed as a part of
  EThread::execute() loop. This is the reason that Net System is a sub-system.

  By get_NetHandler(this_ethread()), you can get the NetHandler object that
  runs inside the current EThread and then @c startIO / @c stopIO which
  assign/release a NetEvent to/from NetHandler. Before you call these functions,
  holding the mutex of this NetHandler is required.

  The NetVConnection provides a set of do_io functions through which you can
  specify continuations to be called back by its NetHandler. These function
  calls do not block. Instead they return an VIO object and schedule the
  callback to the continuation passed in when there are I/O events occurred.

  Multi-thread scheduler:

  The NetHandler should be viewed as multi-threaded schedulers which process
  NetEvents from their queues. If vc wants to be managed by NetHandler, the vc
  should be derived from NetEvent. The vc can be made of NetProcessor (allocate_vc)
  either by directly adding a NetEvent to the queue (NetHandler::startIO), or more
  conveniently, calling a method service call (NetProcessor::connect_re) which
  synthesizes the NetEvent and places it in the queue.

  Callback event codes:

  These event codes for do_io_read and reenable(read VIO) task:
    VC_EVENT_READ_READY, VC_EVENT_READ_COMPLETE,
    VC_EVENT_EOS, VC_EVENT_ERROR

  These event codes for do_io_write and reenable(write VIO) task:
    VC_EVENT_WRITE_READY, VC_EVENT_WRITE_COMPLETE
    VC_EVENT_ERROR

  There is no event and callback for do_io_shutdown / do_io_close task.

  NetVConnection allocation policy:

  VCs are allocated by the NetProcessor and deallocated by NetHandler.
  A state machine may access the returned, non-recurring NetEvent / VIO until
  it is closed by do_io_close. For recurring NetEvent, the NetEvent may be
  accessed until it is closed. Once the NetEvent is closed, it's the
  NetHandler's responsibility to deallocate it.

  Before assign to NetHandler or after release from NetHandler, it's the
  NetEvent's responsibility to deallocate itself.

 */

//
// NetHandler
//
// A NetHandler handles the Network IO operations.  It maintains
// lists of operations at multiples of it's periodicity.
//
class NetHandler : public Continuation, public EThread::LoopTailHandler
{
  using self_type = NetHandler; ///< Self reference type.
public:
  // @a thread and @a trigger_event are redundant - you can get the former from the latter.
  // If we don't get rid of @a trigger_event we should remove @a thread.
  EThread *thread      = nullptr;
  Event *trigger_event = nullptr;
  QueM(NetEvent, NetState, read, ready_link) read_ready_list;
  QueM(NetEvent, NetState, write, ready_link) write_ready_list;
  Que(NetEvent, open_link) open_list;
  DList(NetEvent, cop_link) cop_list;
  ASLLM(NetEvent, NetState, read, enable_link) read_enable_list;
  ASLLM(NetEvent, NetState, write, enable_link) write_enable_list;
  Que(NetEvent, keep_alive_queue_link) keep_alive_queue;
  uint32_t keep_alive_queue_size = 0;
  Que(NetEvent, active_queue_link) active_queue;
  uint32_t active_queue_size = 0;

#ifdef TS_USE_LINUX_IO_URING
  EventIO uring_evio;
#endif

  /// configuration settings for managing the active and keep-alive queues
  struct Config {
    uint32_t max_connections_in                 = 0;
    uint32_t max_requests_in                    = 0;
    uint32_t inactive_threshold_in              = 0;
    uint32_t transaction_no_activity_timeout_in = 0;
    uint32_t keep_alive_no_activity_timeout_in  = 0;
    uint32_t default_inactivity_timeout         = 0;

    /** Return the address of the first value in this struct.

        Doing updates is much easier if we treat this config struct as an array.
        Making it a method means the knowledge of which member is the first one
        is localized to this struct, not scattered about.
     */
    uint32_t &
    operator[](int n)
    {
      return *(&max_connections_in + n);
    }
  };
  /** Static global config, set and updated per process.

      This is updated asynchronously and then events are sent to the NetHandler instances per thread
      to copy to the per thread config at a convenient time. Because these are updated independently
      from the command line, the update events just copy a single value from the global to the
      local. This mechanism relies on members being identical types.
  */
  static Config global_config;
  Config config; ///< Per thread copy of the @c global_config
  // Active and keep alive queue values that depend on other configuration values.
  // These are never updated directly, they are computed from other config values.
  uint32_t max_connections_per_thread_in = 0;
  uint32_t max_requests_per_thread_in    = 0;
  /// Number of configuration items in @c Config.
  static constexpr int CONFIG_ITEM_COUNT = sizeof(Config) / sizeof(uint32_t);
  /// Which members of @c Config the per thread values depend on.
  /// If one of these is updated, the per thread values must also be updated.
  static const std::bitset<CONFIG_ITEM_COUNT> config_value_affects_per_thread_value;
  /// Set of thread types in which nethandlers are active.
  /// This enables signaling the correct instances when the configuration is updated.
  /// Event type threads that use @c NetHandler must set the corresponding bit.
  static std::bitset<std::numeric_limits<unsigned int>::digits> active_thread_types;

  int mainNetEvent(int event, Event *data);
  int waitForActivity(ink_hrtime timeout) override;
  void process_enabled_list();
  void process_ready_list();
  void manage_keep_alive_queue();
  bool manage_active_queue(NetEvent *ne, bool ignore_queue_size);
  void add_to_keep_alive_queue(NetEvent *ne);
  void remove_from_keep_alive_queue(NetEvent *ne);
  bool add_to_active_queue(NetEvent *ne);
  void remove_from_active_queue(NetEvent *ne);

  /// Per process initialization logic.
  static void init_for_process();
  /// Update configuration values that are per thread and depend on other configuration values.
  void configure_per_thread_values();

  /**
    Start to handle read & write event on a NetEvent.
    Initial the socket fd of ne for polling system.
    Only be called when holding the mutex of this NetHandler.

    @param ne NetEvent to be managed by this NetHandler.
    @return 0 on success, ne->nh set to this NetHandler.
            -ERRNO on failure.
   */
  int startIO(NetEvent *ne);
  /**
    Stop to handle read & write event on a NetEvent.
    Remove the socket fd of ne from polling system.
    Only be called when holding the mutex of this NetHandler and must call stopCop(ne) first.

    @param ne NetEvent to be released.
    @return ne->nh set to nullptr.
   */
  void stopIO(NetEvent *ne);

  /**
    Start to handle active timeout and inactivity timeout on a NetEvent.
    Put the ne into open_list. All NetEvents in the open_list is checked for timeout by InactivityCop.
    Only be called when holding the mutex of this NetHandler and must call startIO(ne) first.

    @param ne NetEvent to be managed by InactivityCop
   */
  void startCop(NetEvent *ne);
  /**
    Stop to handle active timeout and inactivity on a NetEvent.
    Remove the ne from open_list and cop_list.
    Also remove the ne from keep_alive_queue and active_queue if its context is IN.
    Only be called when holding the mutex of this NetHandler.

    @param ne NetEvent to be released.
   */
  void stopCop(NetEvent *ne);

  // Signal the epoll_wait to terminate.
  void signalActivity() override;

  /**
    Release a ne and free it.

    @param ne NetEvent to be detached.
   */
  void free_netevent(NetEvent *ne);

  NetHandler();

private:
  void _close_ne(NetEvent *ne, ink_hrtime now, int &handle_event, int &closed, int &total_idle_time, int &total_idle_count);

  /// Static method used as the callback for runtime configuration updates.
  static int update_nethandler_config(const char *name, RecDataT, RecData data, void *);
};

static inline NetHandler *
get_NetHandler(EThread *t)
{
  return (NetHandler *)ETHREAD_GET_PTR(t, unix_netProcessor.netHandler_offset);
}
static inline PollCont *
get_PollCont(EThread *t)
{
  return (PollCont *)ETHREAD_GET_PTR(t, unix_netProcessor.pollCont_offset);
}
static inline PollDescriptor *
get_PollDescriptor(EThread *t)
{
  PollCont *p = get_PollCont(t);
  return p->pollDescriptor;
}

enum ThrottleType {
  ACCEPT,
  CONNECT,
};

TS_INLINE int
net_connections_to_throttle(ThrottleType t)
{
  double headroom = t == ACCEPT ? NET_THROTTLE_ACCEPT_HEADROOM : NET_THROTTLE_CONNECT_HEADROOM;
  int64_t sval    = 0;

  NET_READ_GLOBAL_DYN_SUM(net_connections_currently_open_stat, sval);
  int currently_open = (int)sval;
  // deal with race if we got to multiple net threads
  if (currently_open < 0)
    currently_open = 0;
  return (int)(currently_open * headroom);
}

TS_INLINE void
check_shedding_warning()
{
  ink_hrtime t = Thread::get_hrtime();
  if (t - last_shedding_warning > NET_THROTTLE_MESSAGE_EVERY) {
    last_shedding_warning = t;
    Warning("number of connections reaching shedding limit");
  }
}

TS_INLINE bool
check_net_throttle(ThrottleType t)
{
  int connections = net_connections_to_throttle(t);

  if (net_connections_throttle != 0 && connections >= net_connections_throttle)
    return true;

  return false;
}

TS_INLINE void
check_throttle_warning(ThrottleType type)
{
  ink_hrtime t = Thread::get_hrtime();
  if (t - last_throttle_warning > NET_THROTTLE_MESSAGE_EVERY) {
    last_throttle_warning = t;
    int connections       = net_connections_to_throttle(type);
    Warning("too many connections, throttling.  connection_type=%s, current_connections=%d, net_connections_throttle=%d",
            type == ACCEPT ? "ACCEPT" : "CONNECT", connections, net_connections_throttle);
  }
}

TS_INLINE int
change_net_connections_throttle(const char *token, RecDataT data_type, RecData value, void *data)
{
  (void)token;
  (void)data_type;
  (void)value;
  (void)data;
  int throttle = fds_limit - THROTTLE_FD_HEADROOM;
  if (fds_throttle == 0) {
    net_connections_throttle = fds_throttle;
  } else if (fds_throttle < 0) {
    net_connections_throttle = throttle;
  } else {
    net_connections_throttle = fds_throttle;
    if (net_connections_throttle > throttle)
      net_connections_throttle = throttle;
  }
  return 0;
}

// 2  - ignore
// 1  - transient
// 0  - report as warning
// -1 - fatal
TS_INLINE int
accept_error_seriousness(int res)
{
  switch (res) {
  case -ECONNABORTED:
    return 2;
  case -EAGAIN:
  case -ECONNRESET: // for Linux
  case -EPIPE:      // also for Linux
    return 1;
  case -EMFILE:
  case -ENOMEM:
#if defined(ENOSR) && !defined(freebsd)
  case -ENOSR:
#endif
    ink_assert(!"throttling misconfigured: set too high");
#ifdef ENOBUFS
  // fallthrough
  case -ENOBUFS:
#endif
#ifdef ENFILE
  case -ENFILE:
#endif
    return 0;
  case -EINTR:
    ink_assert(!"should be handled at a lower level");
    return 0;
#if defined(EPROTO) && !defined(freebsd)
  case -EPROTO:
#endif
  case -EOPNOTSUPP:
  case -ENOTSOCK:
  case -ENODEV:
  case -EBADF:
  default:
    return -1;
  }
}

TS_INLINE void
check_transient_accept_error(int res)
{
  ink_hrtime t = Thread::get_hrtime();
  if (!last_transient_accept_error || t - last_transient_accept_error > TRANSIENT_ACCEPT_ERROR_MESSAGE_EVERY) {
    last_transient_accept_error = t;
    Warning("accept thread received transient error: errno = %d", -res);
#if defined(linux)
    if (res == -ENOBUFS || res == -ENFILE)
      Warning("errno : %d consider a memory upgrade", -res);
#endif
  }
}

/** Disable reading on the NetEvent @a ne.
     @param nh Nethandler that owns @a ne.
     @param ne The @c NetEvent to modify.

     - If write is already disable, also disable the inactivity timeout.
     - clear read enabled flag.
     - Remove the @c epoll READ flag.
     - Take @a ne out of the read ready list.
*/
static inline void
read_disable(NetHandler *nh, NetEvent *ne)
{
  if (!ne->write.enabled) {
    // Clear the next scheduled inactivity time, but don't clear inactivity_timeout_in,
    // so the current timeout is used when the NetEvent is reenabled and not the default inactivity timeout
    ne->next_inactivity_timeout_at = 0;
    Debug("socket", "read_disable updating inactivity_at %" PRId64 ", NetEvent=%p", ne->next_inactivity_timeout_at, ne);
  }
  ne->read.enabled = 0;
  nh->read_ready_list.remove(ne);
  ne->ep.modify(-EVENTIO_READ);
}

/** Disable writing on the NetEvent @a ne.
     @param nh Nethandler that owns @a ne.
     @param ne The @c NetEvent to modify.

     - If read is already disable, also disable the inactivity timeout.
     - clear write enabled flag.
     - Remove the @c epoll WRITE flag.
     - Take @a ne out of the write ready list.
*/
static inline void
write_disable(NetHandler *nh, NetEvent *ne)
{
  if (!ne->read.enabled) {
    // Clear the next scheduled inactivity time, but don't clear inactivity_timeout_in,
    // so the current timeout is used when the NetEvent is reenabled and not the default inactivity timeout
    ne->next_inactivity_timeout_at = 0;
    Debug("socket", "write_disable updating inactivity_at %" PRId64 ", NetEvent=%p", ne->next_inactivity_timeout_at, ne);
  }
  ne->write.enabled = 0;
  nh->write_ready_list.remove(ne);
  ne->ep.modify(-EVENTIO_WRITE);
}

TS_INLINE int
EventIO::start(EventLoop l, DNSConnection *vc, int events)
{
  type        = EVENTIO_DNS_CONNECTION;
  data.dnscon = vc;
  return start_common(l, vc->fd, events);
}
TS_INLINE int
EventIO::start(EventLoop l, NetAccept *vc, int events)
{
  type    = EVENTIO_NETACCEPT;
  data.na = vc;
  return start_common(l, vc->server.fd, events);
}
TS_INLINE int
EventIO::start(EventLoop l, NetEvent *ne, int events)
{
  type    = EVENTIO_READWRITE_VC;
  data.ne = ne;
  return start_common(l, ne->get_fd(), events);
}

TS_INLINE int
EventIO::start(EventLoop l, UnixUDPConnection *vc, int events)
{
  type    = EVENTIO_UDP_CONNECTION;
  data.uc = vc;
  return start_common(l, vc->fd, events);
}

TS_INLINE int
EventIO::close()
{
  if (!this->syscall) {
    return 0;
  }

  stop();
  switch (type) {
  default:
    ink_assert(!"case");
  // fallthrough
  case EVENTIO_DNS_CONNECTION:
    return data.dnscon->close();
    break;
  case EVENTIO_NETACCEPT:
    return data.na->server.close();
    break;
  case EVENTIO_READWRITE_VC:
    return data.ne->close();
    break;
  }
  return -1;
}

TS_INLINE int
EventIO::start(EventLoop l, int afd, NetEvent *ne, int e)
{
  data.ne = ne;
  return start_common(l, afd, e);
}

TS_INLINE int
EventIO::start_common(EventLoop l, int afd, int e)
{
  if (!this->syscall) {
    return 0;
  }

  fd         = afd;
  event_loop = l;
#if TS_USE_EPOLL
  struct epoll_event ev;
  memset(&ev, 0, sizeof(ev));
  ev.events   = e | EPOLLEXCLUSIVE;
  ev.data.ptr = this;
#ifndef USE_EDGE_TRIGGER
  events = e;
#endif
  return epoll_ctl(event_loop->epoll_fd, EPOLL_CTL_ADD, fd, &ev);
#endif
#if TS_USE_KQUEUE
  events = e;
  struct kevent ev[2];
  int n = 0;
  if (e & EVENTIO_READ)
    EV_SET(&ev[n++], fd, EVFILT_READ, EV_ADD | INK_EV_EDGE_TRIGGER, 0, 0, this);
  if (e & EVENTIO_WRITE)
    EV_SET(&ev[n++], fd, EVFILT_WRITE, EV_ADD | INK_EV_EDGE_TRIGGER, 0, 0, this);
  return kevent(l->kqueue_fd, &ev[0], n, nullptr, 0, nullptr);
#endif
}

TS_INLINE int
EventIO::modify(int e)
{
  if (!this->syscall) {
    return 0;
  }

  ink_assert(event_loop);
#if TS_USE_EPOLL && !defined(USE_EDGE_TRIGGER)
  struct epoll_event ev;
  memset(&ev, 0, sizeof(ev));
  int new_events = events, old_events = events;
  if (e < 0)
    new_events &= ~(-e);
  else
    new_events |= e;
  events      = new_events;
  ev.events   = new_events;
  ev.data.ptr = this;
  if (!new_events)
    return epoll_ctl(event_loop->epoll_fd, EPOLL_CTL_DEL, fd, &ev);
  else if (!old_events)
    return epoll_ctl(event_loop->epoll_fd, EPOLL_CTL_ADD, fd, &ev);
  else
    return epoll_ctl(event_loop->epoll_fd, EPOLL_CTL_MOD, fd, &ev);
#endif
#if TS_USE_KQUEUE && !defined(USE_EDGE_TRIGGER)
  int n = 0;
  struct kevent ev[2];
  int ee = events;
  if (e < 0) {
    ee &= ~(-e);
    if ((-e) & EVENTIO_READ)
      EV_SET(&ev[n++], fd, EVFILT_READ, EV_DELETE, 0, 0, this);
    if ((-e) & EVENTIO_WRITE)
      EV_SET(&ev[n++], fd, EVFILT_WRITE, EV_DELETE, 0, 0, this);
  } else {
    ee |= e;
    if (e & EVENTIO_READ)
      EV_SET(&ev[n++], fd, EVFILT_READ, EV_ADD | INK_EV_EDGE_TRIGGER, 0, 0, this);
    if (e & EVENTIO_WRITE)
      EV_SET(&ev[n++], fd, EVFILT_WRITE, EV_ADD | INK_EV_EDGE_TRIGGER, 0, 0, this);
  }
  events = ee;
  if (n)
    return kevent(event_loop->kqueue_fd, &ev[0], n, nullptr, 0, nullptr);
  else
    return 0;
#endif
  (void)e; // ATS_UNUSED
  return 0;
}

TS_INLINE int
EventIO::refresh(int e)
{
  if (!this->syscall) {
    return 0;
  }

  ink_assert(event_loop);
#if TS_USE_KQUEUE && defined(USE_EDGE_TRIGGER)
  e = e & events;
  struct kevent ev[2];
  int n = 0;
  if (e & EVENTIO_READ)
    EV_SET(&ev[n++], fd, EVFILT_READ, EV_ADD | INK_EV_EDGE_TRIGGER, 0, 0, this);
  if (e & EVENTIO_WRITE)
    EV_SET(&ev[n++], fd, EVFILT_WRITE, EV_ADD | INK_EV_EDGE_TRIGGER, 0, 0, this);
  if (n)
    return kevent(event_loop->kqueue_fd, &ev[0], n, nullptr, 0, nullptr);
  else
    return 0;
#endif
  (void)e; // ATS_UNUSED
  return 0;
}

TS_INLINE int
EventIO::stop()
{
  if (!this->syscall) {
    return 0;
  }
  if (event_loop) {
    int retval = 0;
#if TS_USE_EPOLL
    struct epoll_event ev;
    memset(&ev, 0, sizeof(struct epoll_event));
    ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
    retval    = epoll_ctl(event_loop->epoll_fd, EPOLL_CTL_DEL, fd, &ev);
#endif
    event_loop = nullptr;
    return retval;
  }
  return 0;
}

TS_INLINE int
NetHandler::startIO(NetEvent *ne)
{
  ink_assert(this->mutex->thread_holding == this_ethread());
  ink_assert(ne->get_thread() == this_ethread());
  int res = 0;

  PollDescriptor *pd = get_PollDescriptor(this->thread);
  if (ne->ep.start(pd, ne, EVENTIO_READ | EVENTIO_WRITE) < 0) {
    res = errno;
    // EEXIST should be ok, though it should have been cleared before we got back here
    if (errno != EEXIST) {
      Debug("iocore_net", "NetHandler::startIO : failed on EventIO::start, errno = [%d](%s)", errno, strerror(errno));
      return -res;
    }
  }

  if (ne->read.triggered == 1) {
    read_ready_list.enqueue(ne);
  }
  ne->nh = this;
  return res;
}

TS_INLINE void
NetHandler::stopIO(NetEvent *ne)
{
  ink_release_assert(ne->nh == this);

  ne->ep.stop();

  read_ready_list.remove(ne);
  write_ready_list.remove(ne);
  if (ne->read.in_enabled_list) {
    read_enable_list.remove(ne);
    ne->read.in_enabled_list = 0;
  }
  if (ne->write.in_enabled_list) {
    write_enable_list.remove(ne);
    ne->write.in_enabled_list = 0;
  }

  ne->nh = nullptr;
}

TS_INLINE void
NetHandler::startCop(NetEvent *ne)
{
  ink_assert(this->mutex->thread_holding == this_ethread());
  ink_release_assert(ne->nh == this);
  ink_assert(!open_list.in(ne));

  open_list.enqueue(ne);
}

TS_INLINE void
NetHandler::stopCop(NetEvent *ne)
{
  ink_release_assert(ne->nh == this);

  open_list.remove(ne);
  cop_list.remove(ne);
  remove_from_keep_alive_queue(ne);
  remove_from_active_queue(ne);
}
