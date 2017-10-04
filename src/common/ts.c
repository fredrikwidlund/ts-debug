#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <dynamic.h>

#include "ts_adaptation_field.h"
#include "ts_pat.h"
#include "ts_pmt.h"
#include "ts_pes.h"
#include "ts_packet.h"
#include "ts_packets.h"
#include "ts_stream.h"
#include "ts.h"

static void ts_streams_release(void *object)
{
  ts_stream *s = *(ts_stream **) object;

  ts_stream_destruct(s);
  free(s);
}

static ssize_t ts_update_pat(ts *ts, ts_pat *pat)
{
  ts_stream *s;

  s = ts_create(ts, pat->program_pid);
  if (s->type == TS_STREAM_TYPE_UNDEFINED)
    ts_stream_type(s, TS_STREAM_TYPE_PSI, 0x02);
  if (s->type != TS_STREAM_TYPE_PSI || s->id != 0x02)
    return -1;

  ts_pat_debug(pat, stderr);

  return 0;
}

static ssize_t ts_update_pmt(ts *ts, ts_pmt *pmt)
{
  ts_pmt_stream *pmt_stream;
  ts_stream *s;

  list_foreach(&pmt->streams, pmt_stream)
    {
      s = ts_create(ts, pmt_stream->elementary_pid);
      if (s->type == TS_STREAM_TYPE_UNDEFINED)
        ts_stream_type(s, TS_STREAM_TYPE_PES, pmt_stream->stream_type);
      if (s->type != TS_STREAM_TYPE_PES || s->id != pmt_stream->stream_type)
        return -1;
    }

  ts_pmt_debug(pmt, stderr);

  return 0;
}

static ssize_t ts_process_pat(ts *ts)
{
  ts_stream **i, *stream;
  ts_unit *unit;
  ts_pat pat;
  ssize_t n;

  list_foreach(ts_streams(ts), i)
    {
      stream = *i;
      if (stream->type == TS_STREAM_TYPE_PSI && (stream->id == 0x00))
        while (!list_empty(ts_stream_units(stream)))
          {
            unit = *(ts_unit **) list_front(ts_stream_units(stream));
            n = ts_pat_construct_buffer(&pat, &unit->data);
            if (n <= 0)
              return n;
            n = ts_update_pat(ts, &pat);
            ts_pat_destruct(&pat);
            if (n == -1)
              return -1;

            (void) ts_stream_read_unit(stream);
          }
    }

  return 0;
}

static ssize_t ts_process_pmt(ts *ts)
{
  ts_stream **i, *stream;
  ts_unit *unit;
  ts_pmt pmt;
  ssize_t n;

  list_foreach(ts_streams(ts), i)
    {
      stream = *i;
      if (stream->type == TS_STREAM_TYPE_PSI && (stream->id == 0x02))
        while (!list_empty(ts_stream_units(stream)))
          {
            unit = *(ts_unit **) list_front(ts_stream_units(stream));
            n = ts_pmt_construct_buffer(&pmt, &unit->data);
            if (n <= 0)
              return n;
            n = ts_update_pmt(ts, &pmt);
            ts_pmt_destruct(&pmt);
            if (n == -1)
              return -1;

            (void) ts_stream_read_unit(stream);
          }
    }

  return 0;
}

static ssize_t ts_process_pes(ts *ts)
{
  ts_stream **i, *stream;
  ts_unit **unit;
  ts_pes *pes;
  ssize_t n;

  list_foreach(ts_streams(ts), i)
    {
      stream = *i;
      if (stream->type == TS_STREAM_TYPE_PES)
        while (!list_empty(ts_stream_units(stream)))
          {
            unit = list_front(ts_stream_units(stream));
            if (ts->status & TS_STATUS_OPEN && list_next(unit) == list_end(ts_stream_units(stream)))
              break;

            pes = malloc(sizeof *pes);
            if (!pes)
              abort();
            n = ts_pes_construct_buffer(pes, &(*unit)->data);
            if (n <= 0)
              {
                free(pes);
                if (n == -1)
                  return -1;
                break;
              }
            (void) ts_stream_read_unit(stream);
            ts_stream_write_pes(stream, pes);
          }
    }

  return 0;
}

static ssize_t ts_process(ts *ts)
{
  ssize_t n;

  n = ts_process_pat(ts);
  if (n == -1)
    return -1;

  n = ts_process_pmt(ts);
  if (n == -1)
    return -1;

  n = ts_process_pes(ts);
  if (n == -1)
    return -1;

  return 0;
}

void ts_construct(ts *ts)
{
  ts->status = TS_STATUS_OPEN;
  list_construct(&ts->streams);
}

void ts_destruct(ts *ts)
{
  list_destruct(&ts->streams, ts_streams_release);
  *ts = (struct ts) {0};
}

void ts_close(ts *ts)
{
  ts->status = TS_STATUS_CLOSED;
  ts_process(ts);
}

ts_stream *ts_lookup(ts *ts, int pid)
{
  ts_stream **i;

  list_foreach(ts_streams(ts), i)
    if ((*i)->pid == pid)
      return *i;
  return NULL;
}

ts_stream *ts_create(ts *ts, int pid)
{
  ts_stream *s, **i;

  s = ts_lookup(ts, pid);
  if (s)
    return s;

  s = malloc(sizeof *s);
  if (!s)
    abort();
  ts_stream_construct(s, pid);

  if (pid == 0)
    ts_stream_type(s, TS_STREAM_TYPE_PSI, 0x00);

  list_foreach(ts_streams(ts), i)
  if (pid < (*i)->pid)
    break;
  list_insert(i, &s, sizeof s);

  return s;
}

list *ts_streams(ts *ts)
{
  return &ts->streams;
}

ssize_t ts_pack_packets(ts *ts, ts_packets *packets)
{
  ts_stream **i;
  ssize_t n, count;

  count = 0;
  list_foreach(ts_streams(ts), i)
    {
      n = ts_stream_pack(*i, packets);
      if (n == -1)
        return -1;
      count += n;
    }

  return count;
}

ssize_t ts_pack_stream(ts *ts, stream *stream)
{
  ts_packets packets;
  ssize_t n;

  ts_packets_construct(&packets);
  n = ts_pack_packets(ts, &packets);
  if (n > 0)
    n = ts_packets_pack(&packets, stream);
  ts_packets_destruct(&packets);
  return n;
}

ssize_t ts_pack_buffer(ts *ts, buffer *buffer)
{
  stream stream;
  ssize_t n;

  stream_construct_buffer(&stream, buffer);
  n = ts_pack_stream(ts, &stream);
  stream_destruct(&stream);
  return n;
}

ssize_t ts_unpack_packets(ts *ts, ts_packets *packets)
{
  ts_packet **i;
  ts_stream *s;
  ssize_t n, count;
  int e;

  count = 0;
  list_foreach(ts_packets_list(packets), i)
    {
      if ((*i)->pid == 0x1fff)
        continue;
      s = ts_create(ts, (*i)->pid);
      n = ts_stream_unpack(s, *i);
      if (n == -1)
        return -1;
      count += n;
    }

  e = ts_process(ts);
  if (e == -1)
    return -1;

  return count;
}

ssize_t ts_unpack_stream(ts *ts, stream *stream)
{
  ts_packets packets;
  ssize_t n;

  ts_packets_construct(&packets);
  n = ts_packets_unpack(&packets, stream);
  if (n > 0)
    n = ts_unpack_packets(ts, &packets);
  ts_packets_destruct(&packets);

  return n > 0 ? 1 : n;
}

ssize_t ts_unpack_buffer(ts *ts, buffer *buffer)
{
  stream stream;
  ssize_t n;

  stream_construct_buffer(&stream, buffer);
  n = ts_unpack_stream(ts, &stream);
  stream_destruct(&stream);
  return n;
}

void ts_debug(ts *ts)
{
  ts_stream **i;
  ts_unit **u;
  ts_pes **pes;
  uint8_t *data;
  uint32_t sum;
  size_t units, size, j;

  list_foreach(ts_streams(ts), i)
    {
      units = 0;
      list_foreach(&(*i)->units, u)
        units ++;
      list_foreach(ts_stream_pes(*i), pes)
        ts_pes_debug(*pes, stderr);

      (void) fprintf(stderr, "pid %d, %lu units, data ", (*i)->pid, units);
      if (list_empty(ts_stream_units(*i)))
        {
          (void) fprintf(stderr, "<none>\n");
          continue;
        }
      u = list_front(&(*i)->units);
      data = buffer_data(&(*u)->data);
      size = buffer_size(&(*u)->data);
      for (j = 0; j < 16 && j < size; j ++)
        (void) fprintf(stderr, "%02x", data[j]);
      sum = 0;
      for (j = 0; j < size; j ++)
        sum += data[j];
      (void) fprintf(stderr, "..., sum %08x\n", sum);
    }
}
