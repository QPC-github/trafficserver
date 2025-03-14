/** @file

  Catch based unit tests for IOBuffer

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

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "tscore/I_Layout.h"

#include "I_EventSystem.h"
#include "records/I_RecordsConfig.h"

#include "diags.i"

#define TEST_THREADS 1

TEST_CASE("MIOBuffer", "[iocore]")
{
  SECTION("new_MIOBuffer 100 times")
  {
    int64_t read_avail_len1 = 0;
    int64_t read_avail_len2 = 0;

    for (unsigned i = 0; i < 100; ++i) {
      MIOBuffer *b1            = new_MIOBuffer(BUFFER_SIZE_INDEX_512);
      int64_t len1             = b1->write_avail();
      IOBufferReader *b1reader = b1->alloc_reader();
      b1->fill(len1);
      read_avail_len1 += b1reader->read_avail();

      MIOBuffer *b2            = new_MIOBuffer(BUFFER_SIZE_INDEX_4K);
      int64_t len2             = b2->write_avail();
      IOBufferReader *b2reader = b2->alloc_reader();
      b2->fill(len2);
      read_avail_len2 += b2reader->read_avail();

      free_MIOBuffer(b2);
      free_MIOBuffer(b1);
    }

    CHECK(read_avail_len1 == 100 * BUFFER_SIZE_FOR_INDEX(BUFFER_SIZE_INDEX_512));
    CHECK(read_avail_len2 == 100 * BUFFER_SIZE_FOR_INDEX(BUFFER_SIZE_INDEX_4K));
  }

  SECTION("write")
  {
    MIOBuffer *miob            = new_MIOBuffer(BUFFER_SIZE_INDEX_4K);
    IOBufferReader *miob_r     = miob->alloc_reader();
    const IOBufferBlock *block = miob->first_write_block();

    SECTION("initial state")
    {
      CHECK(miob->size_index == BUFFER_SIZE_INDEX_4K);
      CHECK(miob->water_mark == 0);
      CHECK(miob->first_write_block() != nullptr);
      CHECK(miob->block_size() == 4096);
      CHECK(miob->block_write_avail() == 4096);
      CHECK(miob->current_write_avail() == 4096);
      CHECK(miob->write_avail() == 4096);

      CHECK(miob->max_read_avail() == 0);
      CHECK(miob_r->read_avail() == 0);
    }

    SECTION("write(const void *rbuf, int64_t nbytes)")
    {
      SECTION("1K")
      {
        uint8_t buf[1024];
        memset(buf, 0xAA, sizeof(buf));

        int64_t written = miob->write(buf, sizeof(buf));

        REQUIRE(written == sizeof(buf));

        CHECK(miob->block_size() == 4096);
        CHECK(miob->block_write_avail() == 3072);
        CHECK(miob->current_write_avail() == 3072);
        CHECK(miob->write_avail() == 3072);

        CHECK(miob->first_write_block() == block);

        CHECK(miob->max_read_avail() == sizeof(buf));
        CHECK(miob_r->read_avail() == sizeof(buf));
      }

      SECTION("4K")
      {
        uint8_t buf[4096];
        memset(buf, 0xAA, sizeof(buf));

        int64_t written = miob->write(buf, sizeof(buf));

        REQUIRE(written == sizeof(buf));

        CHECK(miob->block_size() == 4096);
        CHECK(miob->block_write_avail() == 0);
        CHECK(miob->current_write_avail() == 0);
        CHECK(miob->write_avail() == 0);

        CHECK(miob->first_write_block() == block);

        CHECK(miob->max_read_avail() == sizeof(buf));
        CHECK(miob_r->read_avail() == sizeof(buf));
      }

      SECTION("5K")
      {
        uint8_t buf[5120];
        memset(buf, 0xAA, sizeof(buf));

        int64_t written = miob->write(buf, sizeof(buf));

        REQUIRE(written == sizeof(buf));

        CHECK(miob->block_size() == 4096);
        CHECK(miob->block_write_avail() == 3072);
        CHECK(miob->current_write_avail() == 3072);
        CHECK(miob->write_avail() == 3072);

        CHECK(miob->first_write_block() != block);

        CHECK(miob->max_read_avail() == sizeof(buf));
        CHECK(miob_r->read_avail() == sizeof(buf));
      }

      SECTION("8K")
      {
        uint8_t buf[8192];
        memset(buf, 0xAA, sizeof(buf));

        int64_t written = miob->write(buf, sizeof(buf));

        REQUIRE(written == sizeof(buf));

        CHECK(miob->block_size() == 4096);
        CHECK(miob->block_write_avail() == 0);
        CHECK(miob->current_write_avail() == 0);
        CHECK(miob->write_avail() == 0);

        CHECK(miob->first_write_block() != block);

        CHECK(miob->max_read_avail() == sizeof(buf));
        CHECK(miob_r->read_avail() == sizeof(buf));
      }
    }

    free_MIOBuffer(miob);
  }

  SECTION("write_avail")
  {
    MIOBuffer *miob        = new_MIOBuffer(BUFFER_SIZE_INDEX_4K);
    IOBufferReader *miob_r = miob->alloc_reader();
    uint8_t buf[8192];
    memset(buf, 0xAA, sizeof(buf));

    // initial state
    CHECK(miob->block_size() == 4096);
    CHECK(miob->current_write_avail() == 4096);
    CHECK(miob->write_avail() == 4096);

    SECTION("water_mark == 0 (default)")
    {
      REQUIRE(miob->water_mark == 0);

      // fill half of the current buffer
      miob->write(buf, 2048);
      CHECK(miob->max_read_avail() == 2048);
      CHECK(miob->current_write_avail() == 2048);
      CHECK(miob->high_water() == true);
      CHECK(miob->current_low_water() == false);
      CHECK(miob->write_avail() == 2048); ///< should have no side effect

      // fill all of the current buffer
      miob->write(buf, 2048);
      CHECK(miob->max_read_avail() == 4096);
      CHECK(miob->current_write_avail() == 0);
      CHECK(miob->high_water() == true);
      CHECK(miob->current_low_water() == true);
      CHECK(miob->write_avail() == 0); ///< should have no side effect

      // consume half of the data
      miob_r->consume(2048);
      CHECK(miob->max_read_avail() == 2048);
      CHECK(miob->current_write_avail() == 0);
      CHECK(miob->high_water() == true);
      CHECK(miob->current_low_water() == true);
      CHECK(miob->write_avail() == 0); ///< should have no side effect

      // consume all of the data
      miob_r->consume(2048);
      CHECK(miob->max_read_avail() == 0);
      CHECK(miob->current_write_avail() == 0);
      CHECK(miob->high_water() == false);
      CHECK(miob->current_low_water() == true);
      CHECK(miob->write_avail() == 4096); ///< should have a side effect: add a new block

      CHECK(miob->max_read_avail() == 0);
      CHECK(miob->current_write_avail() == 4096);
      CHECK(miob->high_water() == false);
      CHECK(miob->current_low_water() == false);
      CHECK(miob->write_avail() == 4096); ///< should have no side effect
    }

    SECTION("water_mark == half of block size")
    {
      miob->water_mark = 2048;
      REQUIRE(miob->water_mark * 2 == miob->block_size());

      // fill half of the current buffer
      miob->write(buf, 2048);
      CHECK(miob->max_read_avail() == 2048);
      CHECK(miob->current_write_avail() == 2048);
      CHECK(miob->high_water() == false);
      CHECK(miob->current_low_water() == true);
      CHECK(miob->write_avail() == 6144); ///< should have a side effect: add a new block

      CHECK(miob->max_read_avail() == 2048);
      CHECK(miob->current_write_avail() == 6144);
      CHECK(miob->high_water() == false);
      CHECK(miob->current_low_water() == false);
      CHECK(miob->write_avail() == 6144); ///< should have no side effect

      // fill all of the current buffer
      miob->write(buf, 6144);
      CHECK(miob->max_read_avail() == 8192);
      CHECK(miob->current_write_avail() == 0);
      CHECK(miob->high_water() == true);
      CHECK(miob->current_low_water() == true);
      CHECK(miob->write_avail() == 0); ///< should have no side effect

      // consume half of the data
      miob_r->consume(4096);
      CHECK(miob->max_read_avail() == 4096);
      CHECK(miob->current_write_avail() == 0);
      CHECK(miob->high_water() == true);
      CHECK(miob->current_low_water() == true);
      CHECK(miob->write_avail() == 0); ///< should have no side effect

      // consume all of the data
      miob_r->consume(4096);
      CHECK(miob->max_read_avail() == 0);
      CHECK(miob->current_write_avail() == 0);
      CHECK(miob->high_water() == false);
      CHECK(miob->current_low_water() == true);
      CHECK(miob->write_avail() == 4096); ///< should have a side effect: add a new block

      CHECK(miob->max_read_avail() == 0);
      CHECK(miob->current_write_avail() == 4096);
      CHECK(miob->high_water() == false);
      CHECK(miob->current_low_water() == false);
      CHECK(miob->write_avail() == 4096); ///< should have no side effect
    }

    SECTION("water_mark == block_size()")
    {
      miob->water_mark = 4096;
      REQUIRE(miob->water_mark == miob->block_size());

      // fill half of the current buffer
      miob->write(buf, 2048);
      CHECK(miob->max_read_avail() == 2048);
      CHECK(miob->current_write_avail() == 2048);
      CHECK(miob->high_water() == false);
      CHECK(miob->current_low_water() == true);
      CHECK(miob->write_avail() == 6144); ///< should have a side effect: add a new block

      CHECK(miob->max_read_avail() == 2048);
      CHECK(miob->current_write_avail() == 6144);
      CHECK(miob->high_water() == false);
      CHECK(miob->current_low_water() == false);
      CHECK(miob->write_avail() == 6144); ///< should have no side effect

      // fill all of the current buffer
      miob->write(buf, 6144);
      CHECK(miob->max_read_avail() == 8192);
      CHECK(miob->current_write_avail() == 0);
      CHECK(miob->high_water() == true);
      CHECK(miob->current_low_water() == true);
      CHECK(miob->write_avail() == 0); ///< should have no side effect

      // consume half of the data
      miob_r->consume(4096);
      CHECK(miob->max_read_avail() == 4096);
      CHECK(miob->current_write_avail() == 0);
      CHECK(miob->high_water() == false);
      CHECK(miob->current_low_water() == true);
      CHECK(miob->write_avail() == 4096); ///< should have a side effect: add a new block
      IOBufferBlock *tail = miob->_writer->next.get();
      CHECK(tail != nullptr);

      CHECK(miob->max_read_avail() == 4096);
      CHECK(miob->current_write_avail() == 4096);
      CHECK(miob->high_water() == false);
      CHECK(miob->current_low_water() == true);
      CHECK(miob->write_avail() == 4096);       ///< should have no side effect
      CHECK(tail == miob->_writer->next.get()); ///< the tail block should not be changed

      // consume all of the data
      miob_r->consume(4096);
      CHECK(miob->max_read_avail() == 0);
      CHECK(miob->current_write_avail() == 4096);
      CHECK(miob->high_water() == false);
      CHECK(miob->current_low_water() == true);
      CHECK(miob->write_avail() == 4096);       ///< should have no side effect
      CHECK(tail == miob->_writer->next.get()); ///< the tail block should not be changed
    }

    free_MIOBuffer(miob);
  }
}

TEST_CASE("block size parser", "[iocore]")
{
  int chunk_sizes[DEFAULT_BUFFER_SIZES] = {0};
  auto reset                            = [&]() {
    for (auto &s : chunk_sizes) {
      s = 0;
    }
  };

  REQUIRE(parse_buffer_chunk_sizes("1 2,3, 4", chunk_sizes));
  REQUIRE(chunk_sizes[TS_IOBUFFER_SIZE_INDEX_128] == 1);
  REQUIRE(chunk_sizes[TS_IOBUFFER_SIZE_INDEX_256] == 2);
  REQUIRE(chunk_sizes[TS_IOBUFFER_SIZE_INDEX_512] == 3);
  REQUIRE(chunk_sizes[TS_IOBUFFER_SIZE_INDEX_1K] == 4);
  reset();

  REQUIRE(parse_buffer_chunk_sizes("256k:1", chunk_sizes));
  REQUIRE(chunk_sizes[TS_IOBUFFER_SIZE_INDEX_256K] == 1);
  reset();
  REQUIRE(chunk_sizes[TS_IOBUFFER_SIZE_INDEX_256K] == 0);

  REQUIRE(parse_buffer_chunk_sizes("1M:1 256k:2,256:5 2M:10", chunk_sizes));
  REQUIRE(chunk_sizes[TS_IOBUFFER_SIZE_INDEX_1M] == 1);
  REQUIRE(chunk_sizes[TS_IOBUFFER_SIZE_INDEX_256K] == 2);
  REQUIRE(chunk_sizes[TS_IOBUFFER_SIZE_INDEX_256] == 5);
  REQUIRE(chunk_sizes[TS_IOBUFFER_SIZE_INDEX_2M] == 10);
  reset();

  REQUIRE(parse_buffer_chunk_sizes("2M:1 256k:2", chunk_sizes));
  REQUIRE(chunk_sizes[TS_IOBUFFER_SIZE_INDEX_2M] == 1);
  REQUIRE(chunk_sizes[TS_IOBUFFER_SIZE_INDEX_256K] == 2);
  reset();

  // leaving out the index token will just move to the next spot
  REQUIRE(parse_buffer_chunk_sizes("1M:1 2", chunk_sizes));
  REQUIRE(chunk_sizes[TS_IOBUFFER_SIZE_INDEX_1M] == 1);
  REQUIRE(chunk_sizes[TS_IOBUFFER_SIZE_INDEX_2M] == 2);

  // test can't go past the end
  REQUIRE(parse_buffer_chunk_sizes("1M:1 2 3", chunk_sizes) == false);

  // bad index token
  REQUIRE(parse_buffer_chunk_sizes("bob:1 2 3", chunk_sizes) == false);
}

struct EventProcessorListener : Catch::TestEventListenerBase {
  using TestEventListenerBase::TestEventListenerBase;

  void
  testRunStarting(Catch::TestRunInfo const &testRunInfo) override
  {
    Layout::create();
    init_diags("", nullptr);
    RecProcessInit();

    LibRecordsConfigInit();

    ink_event_system_init(EVENT_SYSTEM_MODULE_PUBLIC_VERSION);
    eventProcessor.start(TEST_THREADS);

    EThread *main_thread = new EThread;
    main_thread->set_specific();
  }
};

CATCH_REGISTER_LISTENER(EventProcessorListener);
