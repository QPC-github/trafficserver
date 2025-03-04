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

#include <typeinfo>
#include <chrono>

#include <tscore/TSSystemState.h>

//////////////////////////////////////////////////////////////////////
//
// The EThread Class
//
/////////////////////////////////////////////////////////////////////
#include "P_EventSystem.h"

#if HAVE_EVENTFD
#include <sys/eventfd.h>
#endif

struct AIOCallback;

#define NO_HEARTBEAT                  -1
#define THREAD_MAX_HEARTBEAT_MSECONDS 60

// !! THIS MUST BE IN THE ENUM ORDER !!
char const *const EThread::Metrics::Slice::STAT_NAME[] = {
  "proxy.process.eventloop.count",      "proxy.process.eventloop.events", "proxy.process.eventloop.events.min",
  "proxy.process.eventloop.events.max", "proxy.process.eventloop.wait",   "proxy.process.eventloop.time.min",
  "proxy.process.eventloop.time.max"};

int thread_max_heartbeat_mseconds = THREAD_MAX_HEARTBEAT_MSECONDS;

// To define a class inherits from Thread:
//   1) Define an independent thread_local static member
//   2) Override Thread::set_specific() and assign that member and call Thread::set_specific()
//   3) Define this_Xthread() which get thread specific data
//   4) Clear thread specific data at destructor function.
//
// The below comments are copied from I_Thread.h
//
// Additionally, the EThread class (derived from Thread) maintains its
// own independent data. All (and only) the threads created in the Event
// Subsystem have this data.
thread_local EThread *EThread::this_ethread_ptr;

void
EThread::set_specific()
{
  this_ethread_ptr = this;
  Thread::set_specific();
}

EThread::EThread()
{
  memset(thread_private, 0, PER_THREAD_DATA);
}

EThread::EThread(ThreadType att, int anid) : id(anid), tt(att)
{
  memset(thread_private, 0, PER_THREAD_DATA);
#if HAVE_EVENTFD
  evfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (evfd < 0) {
    if (errno == EINVAL) { // flags invalid for kernel <= 2.6.26
      evfd = eventfd(0, 0);
      if (evfd < 0) {
        Fatal("EThread::EThread: %d=eventfd(0,0),errno(%d)", evfd, errno);
      }
    } else {
      Fatal("EThread::EThread: %d=eventfd(0,EFD_NONBLOCK | EFD_CLOEXEC),errno(%d)", evfd, errno);
    }
  }
#else
  ink_release_assert(pipe(evpipe) >= 0);
  fcntl(evpipe[0], F_SETFD, FD_CLOEXEC);
  fcntl(evpipe[0], F_SETFL, O_NONBLOCK);
  fcntl(evpipe[1], F_SETFD, FD_CLOEXEC);
  fcntl(evpipe[1], F_SETFL, O_NONBLOCK);
#endif
}

EThread::EThread(ThreadType att, Event *e) : tt(att), start_event(e)
{
  ink_assert(att == DEDICATED);
  memset(thread_private, 0, PER_THREAD_DATA);
}

// Provide a destructor so that SDK functions which create and destroy
// threads won't have to deal with EThread memory deallocation.
EThread::~EThread()
{
  ink_release_assert(mutex->thread_holding == static_cast<EThread *>(this));
  if (this_ethread_ptr == this) {
    this_ethread_ptr = nullptr;
  }
}

bool
EThread::is_event_type(EventType et)
{
  return (event_types & (1 << static_cast<int>(et))) != 0;
}

void
EThread::set_event_type(EventType et)
{
  event_types |= (1 << static_cast<int>(et));
}

void
EThread::process_event(Event *e, int calling_code)
{
  ink_assert((!e->in_the_prot_queue && !e->in_the_priority_queue));
  WEAK_MUTEX_TRY_LOCK(lock, e->mutex, this);
  if (!lock.is_locked()) {
    e->timeout_at = cur_time + DELAY_FOR_RETRY;
    EventQueueExternal.enqueue_local(e);
  } else {
    if (e->cancelled) {
      MUTEX_RELEASE(lock);
      free_event(e);
      return;
    }
    Continuation *c_temp = e->continuation;

    // Restore the client IP debugging flags
    set_cont_flags(e->continuation->control_flags);

    e->continuation->handleEvent(calling_code, e);
    ink_assert(!e->in_the_priority_queue);
    ink_assert(c_temp == e->continuation);
    MUTEX_RELEASE(lock);
    if (e->period) {
      if (!e->in_the_prot_queue && !e->in_the_priority_queue) {
        if (e->period < 0) {
          e->timeout_at = e->period;
        } else {
          e->timeout_at = Thread::get_hrtime_updated() + e->period;
        }
        EventQueueExternal.enqueue_local(e);
      }
    } else if (!e->in_the_prot_queue && !e->in_the_priority_queue) {
      free_event(e);
    }
  }
}

void
EThread::process_queue(Que(Event, link) * NegativeQueue, int *ev_count, int *nq_count)
{
  Event *e;

  // Move events from the external thread safe queues to the local queue.
  EventQueueExternal.dequeue_external();

  // execute all the available external events that have
  // already been dequeued
  while ((e = EventQueueExternal.dequeue_local())) {
    ++(*ev_count);
    if (e->cancelled) {
      free_event(e);
    } else if (!e->timeout_at) { // IMMEDIATE
      ink_assert(e->period == 0);
      process_event(e, e->callback_event);
    } else if (e->timeout_at > 0) { // INTERVAL
      EventQueue.enqueue(e, cur_time);
    } else { // NEGATIVE
      Event *p = nullptr;
      Event *a = NegativeQueue->head;
      while (a && a->timeout_at > e->timeout_at) {
        p = a;
        a = a->link.next;
      }
      if (!a) {
        NegativeQueue->enqueue(e);
      } else {
        NegativeQueue->insert(e, p);
      }
    }
    ++(*nq_count);
  }
}

void
EThread::execute_regular()
{
  Event *e;
  Que(Event, link) NegativeQueue;
  ink_hrtime next_time;
  ink_hrtime delta;            // time spent in the event loop
  ink_hrtime loop_start_time;  // Time the loop started.
  ink_hrtime loop_finish_time; // Time at the end of the loop.

  // Track this so we can update on boundary crossing.
  auto prev_slice =
    this->metrics.prev_slice(metrics._slice.data() + (ink_get_hrtime_internal() / HRTIME_SECOND) % Metrics::N_SLICES);

  int nq_count;
  int ev_count;

  // A statically initialized instance we can use as a prototype for initializing other instances.
  static const Metrics::Slice SLICE_INIT;

  // give priority to immediate events
  while (!TSSystemState::is_event_system_shut_down()) {
    loop_start_time = Thread::get_hrtime_updated();
    nq_count        = 0; // count # of elements put on negative queue.
    ev_count        = 0; // # of events handled.

    metrics.current_slice = metrics._slice.data() + (loop_start_time / HRTIME_SECOND) % Metrics::N_SLICES;
    if (metrics.current_slice != prev_slice) {
      // I have observed multi-second event loops in production, making this necessary. [amc]
      do {
        // Need @c const_cast to cast away @c volatile
        memcpy(prev_slice = this->metrics.next_slice(prev_slice), &SLICE_INIT, sizeof(SLICE_INIT));
      } while (metrics.current_slice != prev_slice);
      metrics.current_slice->record_loop_start(loop_start_time);
    }
    ++(metrics.current_slice->_count); // loop started, bump count.

    process_queue(&NegativeQueue, &ev_count, &nq_count);

    bool done_one;
    do {
      done_one = false;
      // execute all the eligible internal events
      EventQueue.check_ready(loop_start_time, this);
      while ((e = EventQueue.dequeue_ready(cur_time))) {
        ink_assert(e);
        ink_assert(e->timeout_at > 0);
        if (e->cancelled) {
          free_event(e);
        } else {
          done_one = true;
          process_event(e, e->callback_event);
        }
      }
    } while (done_one);

    // execute any negative (poll) events
    if (NegativeQueue.head) {
      process_queue(&NegativeQueue, &ev_count, &nq_count);

      // execute poll events
      while ((e = NegativeQueue.dequeue())) {
        process_event(e, EVENT_POLL);
      }
    }

    next_time             = EventQueue.earliest_timeout();
    ink_hrtime sleep_time = next_time - Thread::get_hrtime_updated();
    if (sleep_time > 0) {
      if (EventQueueExternal.localQueue.empty()) {
        sleep_time = std::min(sleep_time, HRTIME_MSECONDS(thread_max_heartbeat_mseconds));
      } else {
        // Because of a missed lock, Timed-Event and Negative-Event have been pushed into localQueue for retry in awhile.
        // Therefore, we have to set the limitation of sleep time in order to handle the next retry in time.
        sleep_time = std::min(sleep_time, DELAY_FOR_RETRY);
      }
      ++(metrics.current_slice->_wait);
    } else {
      sleep_time = 0;
    }

    tail_cb->waitForActivity(sleep_time);

    // loop cleanup
    loop_finish_time = Thread::get_hrtime_updated();
    // @a delta can be negative due to time of day adjustments (which apparently happen quite frequently). I
    // tried using the monotonic clock to get around this but it was *very* stuttery (up to hundreds
    // of milliseconds), far too much to be actually used.
    delta = std::max<ink_hrtime>(0, loop_finish_time - loop_start_time);

    metrics.decay();
    metrics.record_loop_time(delta);
    metrics.current_slice->record_event_count(ev_count);
  }
}

//
// void  EThread::execute()
//
// Execute loops forever on:
// Find the earliest event.
// Sleep until the event time or until an earlier event is inserted
// When it's time for the event, try to get the appropriate continuation
// lock. If successful, call the continuation, otherwise put the event back
// into the queue.
//

void
EThread::execute()
{
  // Do the start event first.
  // coverity[lock]
  if (start_event) {
    MUTEX_TAKE_LOCK_FOR(start_event->mutex, this, start_event->continuation);
    start_event->continuation->handleEvent(EVENT_IMMEDIATE, start_event);
    MUTEX_UNTAKE_LOCK(start_event->mutex, this);
    free_event(start_event);
    start_event = nullptr;
  }

  switch (tt) {
  case REGULAR: {
    /* The Event Thread has two status: busy and sleep:
     *   - Keep `EThread::lock` locked while Event Thread is busy,
     *   - The `EThread::lock` is released while Event Thread is sleep.
     * When other threads try to acquire the `EThread::lock` of the target Event Thread:
     *   - Acquired, indicating that the target Event Thread is sleep,
     *   - Failed, indicating that the target Event Thread is busy.
     */
    ink_mutex_acquire(&EventQueueExternal.lock);
    this->execute_regular();
    ink_mutex_release(&EventQueueExternal.lock);
    break;
  }
  case DEDICATED: {
    break;
  }
  default:
    ink_assert(!"bad case value (execute)");
    break;
  } /* End switch */
  // coverity[missing_unlock]
}

EThread::Metrics::Slice &
EThread::Metrics::Slice::operator+=(Slice const &that)
{
  this->_events._max   = std::max(this->_events._max, that._events._max);
  this->_events._min   = std::min(this->_events._min, that._events._min);
  this->_events._total += that._events._total;
  this->_duration._min = std::min(this->_duration._min, that._duration._min);
  this->_duration._max = std::max(this->_duration._max, that._duration._max);
  this->_count         += that._count;
  this->_wait          += that._wait;
  return *this;
}

void
EThread::Metrics::summarize(Metrics &global)
{
  // Accumulate in local first so each sample only needs to be processed once,
  // not N_EVENT_TIMESCALES times.
  Slice sum;

  // To avoid race conditions, we back up one from the current metric block. It's close enough
  // and won't be updated during the time this method runs so it should be thread safe.
  Slice *slice = this->prev_slice(current_slice);

  for (unsigned t = 0; t < N_TIMESCALES; ++t) {
    int count = SLICE_SAMPLE_COUNT[t];
    if (t > 0) {
      count -= SLICE_SAMPLE_COUNT[t - 1];
    }
    while (--count >= 0) {
      if (0 != slice->_duration._start) {
        sum += *slice;
      }
      slice = this->prev_slice(slice);
    }
    global._slice[t] += sum; // push out to return vector.
  }

  // Only summarize if there's no outstanding decay.
  if (0 == _decay_count) {
    global._loop_timing += _loop_timing;
    global._api_timing  += _api_timing;
  }
}
