#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <dynamic.h>

#include "ts_adaptation_field.h"
#include "ts_packet.h"
#include "ts_packets.h"
#include "ts_stream.h"
#include "ts_psi.h"
#include "ts.h"

static void ts_streams_release(void *object)
{
  ts_stream *s = *(ts_stream **) object;

  ts_stream_destruct(s);
  free(s);
}

static ssize_t ts_update_psi(ts *ts, ts_psi *psi)
{
  (void) ts;
  (void) psi;
  return 0;
}

static ssize_t ts_process_psi(ts *ts, ts_stream *stream)
{
  ts_unit **i;
  ts_psi psi;
  ssize_t n;

  list_foreach(ts_stream_units(stream), i)
    {
      ts_psi_construct(&psi);
      n = ts_psi_unpack_buffer(&psi, &(*i)->data);
      if (n <= 0)
        {
          ts_psi_destruct(&psi);
          return n;
        }
      n = ts_update_psi(ts, &psi);
      ts_psi_destruct(&psi);
      if (n == -1)
        return n;
    }

  return 0;
}

static int ts_process(ts *ts)
{
  ts_stream **i;
  int e;
  list_foreach(ts_streams(ts), i)
    if ((*i)->type == TS_STREAM_TYPE_PSI)
      {
        e = ts_process_psi(ts, *i);
        if (e == -1)
          return -1;
      }

  return 0;
}

void ts_construct(ts *ts)
{
  list_construct(&ts->streams);
}

void ts_destruct(ts *ts)
{
  list_destruct(&ts->streams, ts_streams_release);
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

  s = malloc(sizeof *s);
  if (!s)
    abort();
  ts_stream_construct(s, pid);

  if (pid == 0)
    ts_stream_type(s, TS_STREAM_TYPE_PSI);

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

ssize_t ts_pack(ts *ts, stream *stream)
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
      s = ts_lookup(ts, (*i)->pid);
      if (!s)
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

ssize_t ts_unpack(ts *ts, stream *stream)
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

void ts_debug(ts *ts)
{
  ts_stream **i;
  ts_unit **u;
  uint8_t *data;
  uint32_t sum;
  size_t units, size, j;

  list_foreach(ts_streams(ts), i)
    {
      units = 0;
      list_foreach(&(*i)->units, u)
        units ++;
      (void) fprintf(stderr, "pid %d, %lu units, data ", (*i)->pid, units);

      u = list_front(&(*i)->units);
      data = buffer_data(&(*u)->data);
      size = buffer_size(&(*u)->data);
      for (j = 0; j < 16 && j < size; j ++)
        (void) fprintf(stderr, "%02x", data[j]);
      sum = 0;
      for (j = 0; j < size; j ++)
        sum += data[j];
      (void) fprintf(stderr, "  sum %08x\n", sum);
    }
}
