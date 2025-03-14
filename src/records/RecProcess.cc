/** @file

  Record process definitions

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

#include "tscore/ink_platform.h"
#include "tscore/EventNotify.h"

#include "I_Tasks.h"

#include "P_EventSystem.h"
#include "records/P_RecCore.h"
#include "records/P_RecProcess.h"
#include "records/P_RecMessage.h"
#include "records/P_RecUtils.h"
#include "records/P_RecFile.h"

// Marks whether the message handler has been initialized.
static bool message_initialized_p = false;
static bool g_started             = false;
static EventNotify g_force_req_notify;
static int g_rec_raw_stat_sync_interval_ms = REC_RAW_STAT_SYNC_INTERVAL_MS;
static int g_rec_config_update_interval_ms = REC_CONFIG_UPDATE_INTERVAL_MS;
static int g_rec_remote_sync_interval_ms   = REC_REMOTE_SYNC_INTERVAL_MS;
static Event *raw_stat_sync_cont_event;
static Event *config_update_cont_event;
static Event *sync_cont_event;

//-------------------------------------------------------------------------
// i_am_the_record_owner, only used for librecords_p.a
//-------------------------------------------------------------------------
bool
i_am_the_record_owner(RecT rec_type)
{
  switch (rec_type) {
  case RECT_CONFIG:
  case RECT_PROCESS:
  case RECT_NODE:
  case RECT_LOCAL:
  case RECT_PLUGIN:
    return true;
  default:
    ink_assert(!"Unexpected RecT type");
    return false;
  }

  return false;
}

//-------------------------------------------------------------------------
// Simple setters for the intervals to decouple this from the proxy
//-------------------------------------------------------------------------
void
RecProcess_set_raw_stat_sync_interval_ms(int ms)
{
  Debug("statsproc", "g_rec_raw_stat_sync_interval_ms -> %d", ms);
  g_rec_raw_stat_sync_interval_ms = ms;
  if (raw_stat_sync_cont_event) {
    Debug("statsproc", "Rescheduling raw-stat syncer");
    raw_stat_sync_cont_event->schedule_every(HRTIME_MSECONDS(g_rec_raw_stat_sync_interval_ms));
  }
}
void
RecProcess_set_config_update_interval_ms(int ms)
{
  Debug("statsproc", "g_rec_config_update_interval_ms -> %d", ms);
  g_rec_config_update_interval_ms = ms;
  if (config_update_cont_event) {
    Debug("statsproc", "Rescheduling config syncer");
    config_update_cont_event->schedule_every(HRTIME_MSECONDS(g_rec_config_update_interval_ms));
  }
}
void
RecProcess_set_remote_sync_interval_ms(int ms)
{
  Debug("statsproc", "g_rec_remote_sync_interval_ms -> %d", ms);
  g_rec_remote_sync_interval_ms = ms;
  if (sync_cont_event) {
    Debug("statsproc", "Rescheduling remote syncer");
    sync_cont_event->schedule_every(HRTIME_MSECONDS(g_rec_remote_sync_interval_ms));
  }
}

//-------------------------------------------------------------------------
// raw_stat_sync_cont
//-------------------------------------------------------------------------
struct raw_stat_sync_cont : public Continuation {
  raw_stat_sync_cont(ProxyMutex *m) : Continuation(m) { SET_HANDLER(&raw_stat_sync_cont::exec_callbacks); }
  int
  exec_callbacks(int /* event */, Event * /* e */)
  {
    RecExecRawStatSyncCbs();
    Debug("statsproc", "raw_stat_sync_cont() processed");

    return EVENT_CONT;
  }
};

//-------------------------------------------------------------------------
// config_update_cont
//-------------------------------------------------------------------------
struct config_update_cont : public Continuation {
  config_update_cont(ProxyMutex *m) : Continuation(m) { SET_HANDLER(&config_update_cont::exec_callbacks); }
  int
  exec_callbacks(int /* event */, Event * /* e */)
  {
    RecExecConfigUpdateCbs(REC_PROCESS_UPDATE_REQUIRED);
    Debug("statsproc", "config_update_cont() processed");

    return EVENT_CONT;
  }
};

//-------------------------------------------------------------------------
// sync_cont
//-------------------------------------------------------------------------
struct sync_cont : public Continuation {
  TextBuffer *m_tb;

  sync_cont(ProxyMutex *m) : Continuation(m)
  {
    SET_HANDLER(&sync_cont::sync);
    m_tb = new TextBuffer(65536);
  }

  ~sync_cont() override
  {
    if (m_tb != nullptr) {
      delete m_tb;
      m_tb = nullptr;
    }
  }

  int
  sync(int /* event */, Event * /* e */)
  {
    RecSyncStatsFile();

    Debug("statsproc", "sync_cont() processed");

    return EVENT_CONT;
  }
};

//-------------------------------------------------------------------------
// RecProcessInit
//-------------------------------------------------------------------------
int
RecProcessInit(Diags *_diags)
{
  static bool initialized_p = false;

  if (initialized_p) {
    return REC_ERR_OKAY;
  }

  if (RecCoreInit(_diags) == REC_ERR_FAIL) {
    return REC_ERR_FAIL;
  }

  initialized_p = true;

  return REC_ERR_OKAY;
}

void
RecMessageInit()
{
  message_initialized_p = true;
}
//-------------------------------------------------------------------------
// RecProcessInitMessage
//-------------------------------------------------------------------------
int
RecProcessInitMessage()
{
  static bool initialized_p = false;

  if (initialized_p) {
    return REC_ERR_OKAY;
  }

  RecMessageInit();
  initialized_p = true;

  return REC_ERR_OKAY;
}

//-------------------------------------------------------------------------
// RecProcessStart
//-------------------------------------------------------------------------
int
RecProcessStart()
{
  if (g_started) {
    return REC_ERR_OKAY;
  }

  Debug("statsproc", "Starting sync continuations:");
  raw_stat_sync_cont *rssc = new raw_stat_sync_cont(new_ProxyMutex());
  Debug("statsproc", "raw-stat syncer");
  raw_stat_sync_cont_event = eventProcessor.schedule_every(rssc, HRTIME_MSECONDS(g_rec_raw_stat_sync_interval_ms), ET_TASK);

  config_update_cont *cuc = new config_update_cont(new_ProxyMutex());
  Debug("statsproc", "config syncer");
  config_update_cont_event = eventProcessor.schedule_every(cuc, HRTIME_MSECONDS(g_rec_config_update_interval_ms), ET_TASK);

  sync_cont *sc = new sync_cont(new_ProxyMutex());
  Debug("statsproc", "remote syncer");
  sync_cont_event = eventProcessor.schedule_every(sc, HRTIME_MSECONDS(g_rec_remote_sync_interval_ms), ET_TASK);

  g_started = true;

  return REC_ERR_OKAY;
}
