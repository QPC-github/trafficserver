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

#include "Http3Transaction.h"
#include "P_QUICNetVConnection.h"

#include "QUICDebugNames.h"

#include "Http3Session.h"
#include "Http3StreamDataVIOAdaptor.h"
#include "Http3HeaderVIOAdaptor.h"
#include "Http3HeaderFramer.h"
#include "Http3DataFramer.h"
#include "HttpSM.h"

#define Http3TransDebug(fmt, ...)                                                                                            \
  Debug("http3_trans", "[%s] [%" PRIx32 "] " fmt,                                                                            \
        static_cast<QUICConnection *>(reinterpret_cast<QUICNetVConnection *>(this->_proxy_ssn->get_netvc()))->cids().data(), \
        this->get_transaction_id(), ##__VA_ARGS__)

#define Http3TransVDebug(fmt, ...)                                                                                           \
  Debug("v_http3_trans", "[%s] [%" PRIx32 "] " fmt,                                                                          \
        static_cast<QUICConnection *>(reinterpret_cast<QUICNetVConnection *>(this->_proxy_ssn->get_netvc()))->cids().data(), \
        this->get_transaction_id(), ##__VA_ARGS__)

// static void
// dump_io_buffer(IOBufferReader *reader)
// {
//   IOBufferReader *debug_reader = reader->clone();
//   uint8_t msg[1024]            = {0};
//   int64_t msg_len              = 1024;
//   int64_t read_len             = debug_reader->read(msg, msg_len);
//   Debug("v_http3_trans", "len=%" PRId64 "\n%s\n", read_len, msg);
// }

//
// HQTransaction
//
HQTransaction::HQTransaction(HQSession *session, QUICStreamVCAdapter::IOInfo &info) : super(session), _info(info)
{
  this->mutex   = new_ProxyMutex();
  this->_thread = this_ethread();

  this->_reader = this->_read_vio_buf.alloc_reader();
}

HQTransaction::~HQTransaction() {}

void
HQTransaction::set_active_timeout(ink_hrtime timeout_in)
{
  if (this->_proxy_ssn) {
    this->_proxy_ssn->set_active_timeout(timeout_in);
  }
}

void
HQTransaction::set_inactivity_timeout(ink_hrtime timeout_in)
{
  if (this->_proxy_ssn) {
    this->_proxy_ssn->set_inactivity_timeout(timeout_in);
  }
}

void
HQTransaction::cancel_inactivity_timeout()
{
  if (this->_proxy_ssn) {
    this->_proxy_ssn->cancel_inactivity_timeout();
  }
}

void
HQTransaction::release()
{
  this->do_io_close();
  this->_sm = nullptr;
}

VIO *
HQTransaction::do_io_read(Continuation *c, int64_t nbytes, MIOBuffer *buf)
{
  if (buf) {
    this->_read_vio.buffer.writer_for(buf);
  } else {
    this->_read_vio.buffer.clear();
  }

  this->_read_vio.mutex     = c ? c->mutex : this->mutex;
  this->_read_vio.cont      = c;
  this->_read_vio.nbytes    = nbytes;
  this->_read_vio.ndone     = 0;
  this->_read_vio.vc_server = this;
  this->_read_vio.op        = VIO::READ;

  if (buf) {
    this->_process_read_vio();
    this->_send_tracked_event(this->_read_event, VC_EVENT_READ_READY, &this->_read_vio);
  }

  return &this->_read_vio;
}

VIO *
HQTransaction::do_io_write(Continuation *c, int64_t nbytes, IOBufferReader *buf, bool owner)
{
  if (buf) {
    this->_write_vio.buffer.reader_for(buf);
  } else {
    this->_write_vio.buffer.clear();
  }

  this->_write_vio.mutex     = c ? c->mutex : this->mutex;
  this->_write_vio.cont      = c;
  this->_write_vio.nbytes    = nbytes;
  this->_write_vio.ndone     = 0;
  this->_write_vio.vc_server = this;
  this->_write_vio.op        = VIO::WRITE;

  if (c != nullptr && nbytes > 0) {
    // TODO Return nullptr if the stream is not on writable state
    this->_process_write_vio();
    this->_send_tracked_event(this->_write_event, VC_EVENT_WRITE_READY, &this->_write_vio);
  }

  return &this->_write_vio;
}

void
HQTransaction::do_io_close(int lerrno)
{
  if (this->_read_event) {
    this->_read_event->cancel();
    this->_read_event = nullptr;
  }

  if (this->_write_event) {
    this->_write_event->cancel();
    this->_write_event = nullptr;
  }

  this->_read_vio.buffer.clear();
  this->_read_vio.nbytes = 0;
  this->_read_vio.op     = VIO::NONE;
  this->_read_vio.cont   = nullptr;

  this->_write_vio.buffer.clear();
  this->_write_vio.nbytes = 0;
  this->_write_vio.op     = VIO::NONE;
  this->_write_vio.cont   = nullptr;

  this->_proxy_ssn->do_io_close(lerrno);
}

void
HQTransaction::do_io_shutdown(ShutdownHowTo_t howto)
{
  return;
}

void
HQTransaction::reenable(VIO *vio)
{
  if (vio->op == VIO::READ) {
    int64_t len = this->_process_read_vio();
    this->_info.read_vio->reenable();

    if (len > 0) {
      this->_signal_read_event();
    }
  } else if (vio->op == VIO::WRITE) {
    int64_t len = this->_process_write_vio();
    this->_info.write_vio->reenable();

    if (len > 0) {
      this->_signal_write_event();
    }
  }
}

void
HQTransaction::transaction_done()
{
  // TODO: start closing transaction
  super::transaction_done();
  return;
}

int
HQTransaction::get_transaction_id() const
{
  return this->_info.adapter.stream().id();
}

void
HQTransaction::increment_transactions_stat()
{
  // TODO
}

void
HQTransaction::decrement_transactions_stat()
{
  // TODO
}

NetVConnectionContext_t
HQTransaction::direction() const
{
  return this->_proxy_ssn->get_netvc()->get_context();
}

/**
 * @brief Replace existing event only if the new event is different than the inprogress event
 */
Event *
HQTransaction::_send_tracked_event(Event *event, int send_event, VIO *vio)
{
  if (event != nullptr) {
    if (event->callback_event != send_event) {
      event->cancel();
      event = nullptr;
    }
  }

  if (event == nullptr) {
    event = this_ethread()->schedule_imm(this, send_event, vio);
  }

  return event;
}

/**
 * @brief Signal event to this->_read_vio.cont
 */
void
HQTransaction::_signal_read_event()
{
  if (this->_read_vio.cont == nullptr || this->_read_vio.op == VIO::NONE) {
    return;
  }
  int event = this->_read_vio.nbytes == INT64_MAX ? VC_EVENT_READ_READY : VC_EVENT_READ_COMPLETE;

  MUTEX_TRY_LOCK(lock, this->_read_vio.mutex, this_ethread());
  if (lock.is_locked()) {
    this->_read_vio.cont->handleEvent(event, &this->_read_vio);
  } else {
    this_ethread()->schedule_imm(this->_read_vio.cont, event, &this->_read_vio);
  }

  Http3TransVDebug("%s (%d)", get_vc_event_name(event), event);
}

/**
 * @brief Signal event to this->_write_vio.cont
 */
void
HQTransaction::_signal_write_event()
{
  if (this->_write_vio.cont == nullptr || this->_write_vio.op == VIO::NONE) {
    return;
  }
  int event = this->_write_vio.ntodo() ? VC_EVENT_WRITE_READY : VC_EVENT_WRITE_COMPLETE;

  MUTEX_TRY_LOCK(lock, this->_write_vio.mutex, this_ethread());
  if (lock.is_locked()) {
    this->_write_vio.cont->handleEvent(event, &this->_write_vio);
  } else {
    this_ethread()->schedule_imm(this->_write_vio.cont, event, &this->_write_vio);
  }

  Http3TransVDebug("%s (%d)", get_vc_event_name(event), event);
}

//
// Http3Transaction
//
Http3Transaction::Http3Transaction(Http3Session *session, QUICStreamVCAdapter::IOInfo &info) : super(session, info)
{
  static_cast<HQSession *>(this->_proxy_ssn)->add_transaction(static_cast<HQTransaction *>(this));
  QUICStreamId stream_id = this->_info.adapter.stream().id();

  this->_header_framer = new Http3HeaderFramer(this, &this->_write_vio, session->local_qpack(), stream_id);
  this->_data_framer   = new Http3DataFramer(this, &this->_write_vio);
  this->_frame_collector.add_generator(this->_header_framer);
  this->_frame_collector.add_generator(this->_data_framer);
  // this->_frame_collector.add_generator(this->_push_controller);

  HTTPType http_type = HTTP_TYPE_UNKNOWN;
  if (this->direction() == NET_VCONNECTION_OUT) {
    http_type = HTTP_TYPE_RESPONSE;
  } else {
    http_type = HTTP_TYPE_REQUEST;
  }
  this->_header_handler = new Http3HeaderVIOAdaptor(&this->_read_vio, http_type, session->remote_qpack(), stream_id);
  this->_data_handler   = new Http3StreamDataVIOAdaptor(&this->_read_vio);

  this->_frame_dispatcher.add_handler(this->_header_handler);
  this->_frame_dispatcher.add_handler(this->_data_handler);

  SET_HANDLER(&Http3Transaction::state_stream_open);
}

Http3Transaction::~Http3Transaction()
{
  delete this->_header_framer;
  delete this->_data_framer;
  delete this->_header_handler;
  delete this->_data_handler;
}

int
Http3Transaction::state_stream_open(int event, void *edata)
{
  // TODO: should check recursive call?
  if (this->_thread != this_ethread()) {
    // Send on to the owning thread
    if (this->_cross_thread_event == nullptr) {
      this->_cross_thread_event = this->_thread->schedule_imm(this, event, edata);
    }
    return 0;
  }

  SCOPED_MUTEX_LOCK(lock, this->mutex, this_ethread());

  Event *e = static_cast<Event *>(edata);
  if (e == this->_cross_thread_event) {
    this->_cross_thread_event = nullptr;
  }

  switch (event) {
  case VC_EVENT_READ_READY:
    Http3TransVDebug("%s (%d)", get_vc_event_name(event), event);
    // if no progress, don't need to signal
    if (this->_process_read_vio() > 0) {
      this->_signal_read_event();
    }
    this->_info.read_vio->reenable();
    break;
  case VC_EVENT_READ_COMPLETE:
    Http3TransVDebug("%s (%d)", get_vc_event_name(event), event);
    this->_process_read_vio();
    if (!this->_header_handler->is_complete()) {
      // Delay processing READ_COMPLETE
      this_ethread()->schedule_imm(this, VC_EVENT_READ_COMPLETE);
      break;
    }
    this->_data_handler->finalize();
    // always signal regardless of progress
    this->_signal_read_event();
    this->_info.read_vio->reenable();
    break;
  case VC_EVENT_WRITE_READY:
    Http3TransVDebug("%s (%d)", get_vc_event_name(event), event);
    // if no progress, don't need to signal
    if (this->_process_write_vio() > 0) {
      this->_signal_write_event();
    }
    this->_info.write_vio->reenable();
    break;
  case VC_EVENT_WRITE_COMPLETE:
    Http3TransVDebug("%s (%d)", get_vc_event_name(event), event);
    this->_process_write_vio();
    // always signal regardless of progress
    this->_signal_write_event();
    this->_info.write_vio->reenable();
    break;
  case VC_EVENT_EOS:
  case VC_EVENT_ERROR:
  case VC_EVENT_INACTIVITY_TIMEOUT:
  case VC_EVENT_ACTIVE_TIMEOUT: {
    Http3TransVDebug("%s (%d)", get_vc_event_name(event), event);
    break;
  }
  default:
    Http3TransDebug("Unknown event %d", event);
  }

  return EVENT_DONE;
}

int
Http3Transaction::state_stream_closed(int event, void *data)
{
  Http3TransVDebug("%s (%d)", get_vc_event_name(event), event);

  switch (event) {
  case VC_EVENT_READ_READY:
  case VC_EVENT_READ_COMPLETE: {
    // ignore
    break;
  }
  case VC_EVENT_WRITE_READY:
  case VC_EVENT_WRITE_COMPLETE: {
    // ignore
    break;
  }
  case VC_EVENT_EOS:
  case VC_EVENT_ERROR:
  case VC_EVENT_INACTIVITY_TIMEOUT:
  case VC_EVENT_ACTIVE_TIMEOUT: {
    // TODO
    Http3TransVDebug("%s (%d)", get_vc_event_name(event), event);
    break;
  }
  default:
    Http3TransDebug("Unknown event %d", event);
  }

  return EVENT_DONE;
}

void
Http3Transaction::do_io_close(int lerrno)
{
  SET_HANDLER(&Http3Transaction::state_stream_closed);
  super::do_io_close(lerrno);
}

bool
Http3Transaction::is_response_header_sent() const
{
  return this->_header_framer->is_done();
}

bool
Http3Transaction::is_response_body_sent() const
{
  return this->_data_framer->is_done();
}

int64_t
Http3Transaction::_process_read_vio()
{
  if (this->_info.read_vio->cont == nullptr || this->_info.read_vio->op == VIO::NONE) {
    return 0;
  }

  if (this->_thread != this_ethread()) {
    SCOPED_MUTEX_LOCK(lock, this->mutex, this_ethread());
    if (this->_cross_thread_event == nullptr) {
      // Send to the right thread
      this->_cross_thread_event = this->_thread->schedule_imm(this, VC_EVENT_READ_READY, nullptr);
    }
    return 0;
  }

  SCOPED_MUTEX_LOCK(lock, this->_info.read_vio->mutex, this_ethread());

  uint64_t nread = 0;
  this->_frame_dispatcher.on_read_ready(this->_info.adapter.stream().id(), *this->_info.read_vio->get_reader(), nread);
  this->_info.read_vio->ndone += nread;
  return nread;
}

int64_t
Http3Transaction::_process_write_vio()
{
  if (this->_info.write_vio->cont == nullptr || this->_info.write_vio->op == VIO::NONE) {
    return 0;
  }

  if (this->_thread != this_ethread()) {
    SCOPED_MUTEX_LOCK(lock, this->mutex, this_ethread());
    if (this->_cross_thread_event == nullptr) {
      // Send to the right thread
      this->_cross_thread_event = this->_thread->schedule_imm(this, VC_EVENT_WRITE_READY, nullptr);
    }
    return 0;
  }

  SCOPED_MUTEX_LOCK(lock, this->_info.write_vio->mutex, this_ethread());

  size_t nwritten = 0;
  bool all_done   = false;
  this->_frame_collector.on_write_ready(this->_info.adapter.stream().id(), *this->_info.write_vio->get_writer(), nwritten,
                                        all_done);
  this->_sent_bytes += nwritten;
  if (all_done) {
    this->_info.write_vio->nbytes = this->_sent_bytes;
  }

  return nwritten;
}

// TODO:  Just a place holder for now
bool
Http3Transaction::has_request_body(int64_t content_length, bool is_chunked_set) const
{
  return false;
}

//
// Http09Transaction
//
Http09Transaction::Http09Transaction(Http09Session *session, QUICStreamVCAdapter::IOInfo &info) : super(session, info)
{
  static_cast<HQSession *>(this->_proxy_ssn)->add_transaction(static_cast<HQTransaction *>(this));

  SET_HANDLER(&Http09Transaction::state_stream_open);
}

Http09Transaction::~Http09Transaction() {}

int
Http09Transaction::state_stream_open(int event, void *edata)
{
  // TODO: should check recursive call?
  Http3TransVDebug("%s (%d)", get_vc_event_name(event), event);

  if (this->_thread != this_ethread()) {
    // Send on to the owning thread
    if (this->_cross_thread_event == nullptr) {
      this->_cross_thread_event = this->_thread->schedule_imm(this, event, edata);
    }
    return 0;
  }

  SCOPED_MUTEX_LOCK(lock, this->mutex, this_ethread());

  Event *e = static_cast<Event *>(edata);
  if (e == this->_cross_thread_event) {
    this->_cross_thread_event = nullptr;
  }

  switch (event) {
  case VC_EVENT_READ_READY:
  case VC_EVENT_READ_COMPLETE: {
    int64_t len = this->_process_read_vio();
    // if no progress, don't need to signal
    if (len > 0) {
      this->_signal_read_event();
    }
    this->_info.read_vio->reenable();

    break;
  }
  case VC_EVENT_WRITE_READY:
  case VC_EVENT_WRITE_COMPLETE: {
    int64_t len = this->_process_write_vio();
    if (len > 0) {
      this->_signal_write_event();
    }
    this->_info.write_vio->reenable();

    break;
  }
  case VC_EVENT_EOS:
  case VC_EVENT_ERROR:
  case VC_EVENT_INACTIVITY_TIMEOUT:
  case VC_EVENT_ACTIVE_TIMEOUT: {
    Http3TransDebug("%d", event);
    break;
  }
  default:
    Http3TransDebug("Unknown event %d", event);
  }

  return EVENT_DONE;
}

void
Http09Transaction::do_io_close(int lerrno)
{
  SET_HANDLER(&Http09Transaction::state_stream_closed);
  super::do_io_close(lerrno);
}

int
Http09Transaction::state_stream_closed(int event, void *data)
{
  Http3TransVDebug("%s (%d)", get_vc_event_name(event), event);

  switch (event) {
  case VC_EVENT_READ_READY:
  case VC_EVENT_READ_COMPLETE: {
    // ignore
    break;
  }
  case VC_EVENT_WRITE_READY:
  case VC_EVENT_WRITE_COMPLETE: {
    // ignore
    break;
  }
  case VC_EVENT_EOS:
  case VC_EVENT_ERROR:
  case VC_EVENT_INACTIVITY_TIMEOUT:
  case VC_EVENT_ACTIVE_TIMEOUT: {
    // TODO
    break;
  }
  default:
    Http3TransDebug("Unknown event %d", event);
  }

  return EVENT_DONE;
}

// Convert HTTP/0.9 to HTTP/1.1
int64_t
Http09Transaction::_process_read_vio()
{
  if (this->_read_vio.cont == nullptr || this->_read_vio.op == VIO::NONE) {
    return 0;
  }

  if (this->_thread != this_ethread()) {
    SCOPED_MUTEX_LOCK(lock, this->mutex, this_ethread());
    if (this->_cross_thread_event == nullptr) {
      // Send to the right thread
      this->_cross_thread_event = this->_thread->schedule_imm(this, VC_EVENT_READ_READY, nullptr);
    }
    return 0;
  }

  SCOPED_MUTEX_LOCK(lock, this->_read_vio.mutex, this_ethread());
  IOBufferReader *reader = this->_info.read_vio->get_reader();

  // Nuke this block when we drop 0.9 support
  if (!this->_protocol_detected) {
    uint8_t start[3];
    if (!reader->is_read_avail_more_than(3)) {
      return 0;
    }
    reader->memcpy(start, 3);
    // If the first two bit are 0 and 1, the 3rd byte is type field.
    // Because there is no type value larger than 0x20, we can assume that the
    // request is HTTP/0.9 if the value is larger than 0x20.
    if (0x40 <= start[0] && start[0] < 0x80 && start[2] > 0x20) {
      this->_legacy_request = true;
    }
    this->_protocol_detected = true;
  }

  if (this->_legacy_request) {
    uint64_t nread    = 0;
    MIOBuffer *writer = this->_read_vio.get_writer();
    // Nuke this branch when we drop 0.9 support
    if (!this->_client_req_header_complete) {
      uint8_t buf[4096];
      int len = reader->read(buf, 4096);
      // Check client request is complete or not
      if (len < 2 || buf[len - 1] != '\n') {
        return 0;
      }
      nread                             += len;
      this->_client_req_header_complete = true;

      // Check "CRLF" or "LF"
      int n = 2;
      if (buf[len - 2] != '\r') {
        n = 1;
      }

      writer->write(buf, len - n);
      // FIXME: Get hostname from SNI?
      const char version[] = " HTTP/1.1\r\nHost: localhost\r\n\r\n";
      writer->write(version, sizeof(version));
    } else {
      uint8_t buf[4096];
      int len;
      while ((len = reader->read(buf, 4096)) > 0) {
        nread += len;
        writer->write(buf, len);
      }
    }

    return nread;
    // End of code for HTTP/0.9
  } else {
    // Ignore malformed data
    uint8_t buf[4096];
    int len;
    uint64_t nread = 0;

    while ((len = reader->read(buf, 4096)) > 0) {
      nread += len;
    }

    return nread;
  }
}

// FIXME: already defined somewhere?
static constexpr char http_1_1_version[] = "HTTP/1.1";

// Convert HTTP/1.1 to HTTP/0.9
int64_t
Http09Transaction::_process_write_vio()
{
  if (this->_write_vio.cont == nullptr || this->_write_vio.op == VIO::NONE) {
    return 0;
  }

  if (this->_thread != this_ethread()) {
    SCOPED_MUTEX_LOCK(lock, this->mutex, this_ethread());
    if (this->_cross_thread_event == nullptr) {
      // Send to the right thread
      this->_cross_thread_event = this->_thread->schedule_imm(this, VC_EVENT_WRITE_READY, nullptr);
    }
    return 0;
  }

  SCOPED_MUTEX_LOCK(lock, this->_write_vio.mutex, this_ethread());

  IOBufferReader *reader = this->_write_vio.get_reader();
  if (!reader) {
    return 0;
  }

  if (this->_legacy_request) {
    // This branch is for HTTP/0.9
    int64_t http_1_1_version_len = sizeof(http_1_1_version) - 1;

    if (reader->is_read_avail_more_than(http_1_1_version_len) &&
        memcmp(reader->start(), http_1_1_version, http_1_1_version_len) == 0) {
      // Skip HTTP/1.1 response headers
      IOBufferBlock *headers = reader->get_current_block();
      int64_t headers_size   = headers->read_avail();
      reader->consume(headers_size);
      this->_write_vio.ndone += headers_size;
    }

    // Write HTTP/1.1 response body
    int64_t bytes_avail   = reader->read_avail();
    int64_t total_written = 0;

    while (total_written < bytes_avail) {
      int64_t data_len      = reader->block_read_avail();
      int64_t bytes_written = this->_info.write_vio->get_writer()->write(reader, data_len);
      if (bytes_written <= 0) {
        break;
      }

      reader->consume(bytes_written);
      this->_write_vio.ndone += bytes_written;
      total_written          += bytes_written;
    }

    // NOTE: When Chunked Transfer Coding is supported, check ChunkedState of ChunkedHandler
    // is CHUNK_READ_DONE and set FIN flag
    if (this->_write_vio.ntodo() == 0) {
      // The size of respons to client
      this->_info.write_vio->done();
    }

    return total_written;
  } else {
    // nothing to do
    return 0;
  }
}
