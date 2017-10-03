#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <dynamic.h>

#include "ts_adaptation_field.h"
#include "ts_packet.h"
#include "ts_packets.h"
#include "ts_stream.h"
#include "ts_pat.h"
#include "ts.h"

static void ts_streams_release(void *object)
{
  ts_stream *s = *(ts_stream **) object;

  ts_stream_destruct(s);
  free(s);
}

static ssize_t ts_update_pat(ts *ts, ts_pat *pat)
{
  fprintf(stderr, "update pat\n");
  (void) ts;
  (void) pat;
  return 0;
}

static ssize_t ts_process_pat(ts *ts)
{
  ts_stream **i;
  ts_unit **j;
  ts_pat pat;
  ssize_t n;

  list_foreach(ts_streams(ts), i)
    if ((*i)->type == TS_STREAM_TYPE_PSI && ((*i)->id == 0x00))
      while (!list_empty(ts_stream_units(*i)))
        {
          j = list_front(ts_stream_units(*i));
          ts_pat_construct(&pat);
          n = ts_pat_unpack_buffer(&pat, &(*j)->data);
          if (n <= 0)
            {
              ts_pat_destruct(&pat);
              return n;
            }
          n = ts_update_pat(ts, &pat);
          ts_pat_destruct(&pat);
          if (n == -1)
            return -1;
          (void) ts_stream_read_unit(*i);
        }

  return 0;
}

static ssize_t ts_process(ts *ts)
{
  ssize_t n;

  n = ts_process_pat(ts);
  if (n == -1)
    return -1;
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
