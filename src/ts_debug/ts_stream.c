#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <err.h>

#include <dynamic.h>

#include "bytes.h"
#include "ts_packet.h"
#include "ts_psi.h"
#include "ts_stream.h"

/* ts_unit */

void ts_unit_destruct(ts_unit *unit)
{
  buffer_destruct(&unit->data);
}

ts_unit *ts_unit_new(void)
{
  ts_unit *unit;

  unit = malloc(sizeof *unit);
  if (!unit)
    abort();
  unit->complete = 0;
  buffer_construct(&unit->data);

  return unit;
}

int ts_unit_guess_type(ts_unit *unit)
{
  uint8_t *p;

  if (buffer_size(&unit->data) >= 3)
    {
      p = buffer_data(&unit->data);
      if (p[0] == 0x00 && p[1] == 0x00 && p[2] == 0x01)
        return TS_STREAM_TYPE_PES;
      return TS_STREAM_TYPE_PSI;
    }

  return TS_STREAM_TYPE_UNKNOWN;
}

int ts_unit_write(ts_unit *unit, ts_packet *packet)
{
  buffer_insert(&unit->data, buffer_size(&unit->data), packet->payload_data, packet->payload_size);
  ts_packet_delete(packet);

  return 0;
}

void ts_unit_debug(ts_unit *unit, FILE *f, int indent)
{
  (void) fprintf(f, "%*s[size %lu]\n", indent * 2, "", buffer_size(&unit->data));
}

/* ts_stream */

static void ts_stream_units_release(void *object)
{
  ts_unit_destruct(*(ts_unit **) object);
}

void ts_stream_destruct(ts_stream *stream)
{
  list_destruct(&stream->units, ts_stream_units_release);
}

ts_stream *ts_stream_new(ts_streams *streams, int pid)
{
  ts_stream *stream;

  stream = malloc(sizeof *stream);
  if (!stream)
    abort();
  stream->streams = streams;
  stream->pid = pid;
  stream->type = TS_STREAM_TYPE_UNKNOWN;
  stream->continuity_counter = 0;
  list_construct(&stream->units);

  return stream;
}

void ts_stream_type(ts_stream *stream, int type)
{
  if (stream->type != TS_STREAM_TYPE_UNKNOWN || type == TS_STREAM_TYPE_UNKNOWN)
    return;
  stream->type = type;
}

int ts_streams_lookup_type(ts_streams *streams, int pid)
{
  if (pid == 0)
    return TS_STREAM_TYPE_PSI;

  if (streams->psi.pat.present &&
      streams->psi.pat.program_pid == pid)
    return TS_STREAM_TYPE_PSI;

  return TS_STREAM_TYPE_UNKNOWN;
}

int ts_stream_psi(ts_stream *stream, ts_psi *psi)
{
  ts_streams *streams = stream->streams;

  if (psi->pat.present)
    {
      if (streams->psi.pat.present &&
          streams->psi.pat.version != psi->pat.version)
        return -1;
      streams->psi.pat = psi->pat;
    }

  return 0;
}

int ts_stream_process(ts_stream *stream, ts_unit *unit)
{
  ts_psi psi;
  ssize_t n;
  int e;

  switch (stream->type)
    {
    case TS_STREAM_TYPE_PSI:
      if (unit->complete)
        return 0;

      ts_psi_construct(&psi);
      n = ts_psi_unpack(&psi,  buffer_data(&unit->data), buffer_size(&unit->data));
      e = n > 0 ? ts_stream_psi(stream, &psi) : 0;
      ts_psi_destruct(&psi);
      if (n == -1 || e == -1)
        return -1;

      if (n > 0)
        unit->complete = 1;
      return 0;
    default:
      return 0;
    }
}

int ts_stream_write(ts_stream *stream, ts_packet *packet)
{
  ts_unit *unit;
  int e, empty;


  // ignore NULL packets
  if (packet->pid == 0x1fff)
    {
      ts_packet_delete(packet);
      return 0;
    }

  empty = list_empty(&stream->units);
  // validate and set continuity counters
  if (!empty &&
      packet->adaptation_field_control & 0x01 &&
      ((stream->continuity_counter + 1) & 0x0f) != packet->continuity_counter)
    {
      ts_packet_delete(packet);
      return -1;
    }
  stream->continuity_counter = packet->continuity_counter;

  // create new unit if PUSI is set
  unit = *(ts_unit **) list_back(&stream->units);
  if (packet->payload_unit_start_indicator)
    {
      if (!empty)
        {
          unit->complete = 1;
          e = ts_stream_process(stream, unit);
          if (e == -1)
            return -1;
        }
      unit = ts_unit_new();
      list_push_back(&stream->units, &unit, sizeof unit);
    }
  else if (empty)
    {
      ts_packet_delete(packet);
      return 0;
    }

  // write packet to stream
  e = ts_unit_write(unit, packet);
  if (e == -1)
    return -1;

  return ts_stream_process(stream, unit);
}

void ts_stream_debug(ts_stream *stream, FILE *f, int indent)
{
  ts_unit **i;
  int n;

  (void) fprintf(f, "%*s[pid %d, type %d]\n", indent * 2, "", stream->pid, stream->type);
  n = 0;
  indent ++;
  list_foreach(&stream->units, i)
    {
      (void) fprintf(f, "%*s[unit %d]\n", indent * 2, "", n);
      ts_unit_debug(*i, f, indent + 1);
      n ++;
    }
}

/* ts_streams */

static void ts_streams_streams_release(void *object)
{
  ts_stream_destruct(*(ts_stream **) object);
}

void ts_streams_construct(ts_streams *streams)
{
  list_construct(&streams->streams);
}

void ts_streams_destruct(ts_streams *streams)
{
  list_destruct(&streams->streams, ts_streams_streams_release);
}

ts_stream *ts_streams_lookup(ts_streams *streams, int pid)
{
  ts_stream **i;

  list_foreach(&streams->streams, i)
    if ((*i)->pid == pid)
      return *i;
  return NULL;
}

int ts_streams_write(ts_streams *streams, ts_packets *packets)
{
  ts_packet *packet;
  ts_stream *stream, **i;
  int e;

  while (1)
    {
      packet = ts_packets_read(packets);
      if (!packet)
        return 0;

      stream = ts_streams_lookup(streams, packet->pid);
      if (!stream)
        {
          stream = ts_stream_new(streams, packet->pid);
          ts_stream_type(stream, ts_streams_lookup_type(streams, packet->pid));
          list_foreach(&streams->streams, i)
            if (stream->pid < (*i)->pid)
              break;
          list_insert(i, &stream, sizeof stream);
        }

      e = ts_stream_write(stream, packet);
      if (e == -1)
        return -1;
    }

  return 0;
}

void ts_streams_debug(ts_streams *streams, FILE *f, int indent)
{
  ts_stream **i;
  int n;

  n = 0;
  list_foreach(&streams->streams, i)
    {
      (void) fprintf(f, "%*s[stream %d]\n", indent * 2, "", n);
      ts_stream_debug(*i, f, indent + 1);
      n ++;
    }
}
