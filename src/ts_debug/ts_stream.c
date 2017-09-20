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
#include "ts_pes.h"
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
  ts_pes_construct(&unit->pes);
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
  if (unit->unpacked)
    ts_pes_debug(&unit->pes, f, indent);
  else
    (void) fprintf(f, "%*s[size %lu, complete %u]\n", indent * 2, "", buffer_size(&unit->data), unit->complete);
}

/* ts_stream */

static void ts_stream_units_release(void *object)
{
  ts_unit_destruct(*(ts_unit **) object);
}

void ts_stream_construct(ts_stream *stream, ts_streams *streams, int pid, int type, int content_type)
{
  *stream = (ts_stream) {0};
  stream->streams = streams;
  stream->pid = pid;
  stream->type = type;
  stream->content_type = content_type;
  stream->continuity_counter = 0;
  list_construct(&stream->units);
}

void ts_stream_destruct(ts_stream *stream)
{
  list_destruct(&stream->units, ts_stream_units_release);
}

int ts_stream_type(ts_stream *stream, int type, int content_type)
{
  ts_unit **i;
  int e;

  if (type != TS_STREAM_TYPE_UNKNOWN && stream->type == TS_STREAM_TYPE_UNKNOWN)
    {
      stream->type = type;
      stream->content_type = content_type;
      list_foreach(&stream->units, i)
        {
          e = ts_stream_process(stream, *i);
          if (e == -1)
            return -1;
        }
    }

  return 0;
}

int ts_streams_type(ts_streams *streams, int pid, int type, int content_type)
{
  ts_stream *stream;

  stream = ts_streams_lookup(streams, pid);
  if (stream)
    return ts_stream_type(stream, type, content_type);
  else
    {
      (void) ts_streams_create(streams, pid, type, content_type);
      return 0;
    }
}

int ts_streams_psi(ts_streams *streams, ts_psi *psi)
{
  ts_psi_pmt_stream *i;
  int e;

  if (psi->pat.present && !streams->psi.pat.present)
    {
      streams->psi.pat = psi->pat;
      e = ts_streams_type(streams, streams->psi.pat.program_pid, TS_STREAM_TYPE_PSI, 0);
      if (e == -1)
        return -1;
    }

  if (psi->pmt.present && !streams->psi.pmt.present)
    {
      streams->psi.pmt.present = 1;
      streams->psi.pmt.id_extension = psi->pmt.id_extension;
      streams->psi.pmt.version = psi->pmt.version;
      streams->psi.pmt.pcr_pid = psi->pmt.pcr_pid;
      list_foreach(&psi->pmt.streams, i)
        {
          e = ts_streams_type(streams, i->pid, TS_STREAM_TYPE_PES, i->type);
          if (e == -1)
            return -1;
          list_push_back(&streams->psi.pmt.streams, i, sizeof i);
        }
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
      n = ts_psi_unpack(&psi, buffer_data(&unit->data), buffer_size(&unit->data));
      e = 0;
      if (n > 0)
        e = ts_streams_psi(stream->streams, &psi);
      ts_psi_destruct(&psi);
      if (n == -1 || e == -1)
        return -1;
      if (n > 0)
        unit->complete = 1;
      return 0;
    case TS_STREAM_TYPE_PES:
      if (!unit->complete)
        return 0;
      n = ts_pes_unpack(&unit->pes, buffer_data(&unit->data), buffer_size(&unit->data));
      if (n == -1)
        return -1;
      unit->unpacked = 1;
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

  (void) fprintf(f, "%*s[pid %d, type %d, content type %d]\n", indent * 2, "", stream->pid, stream->type, stream->content_type);
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
  ts_psi_construct(&streams->psi);
  list_construct(&streams->streams);
}

void ts_streams_destruct(ts_streams *streams)
{
  list_destruct(&streams->streams, ts_streams_streams_release);
}

ts_stream *ts_streams_create(ts_streams *streams, int pid, int type, int content_type)
{
  ts_stream *stream, **i;

  stream = malloc(sizeof *stream);
  if (!stream)
    abort();
  if (pid == 0 && type == TS_STREAM_TYPE_UNKNOWN)
    type = TS_STREAM_TYPE_PSI;
  ts_stream_construct(stream, streams, pid, type, content_type);
  list_foreach(&streams->streams, i)
    if (stream->pid < (*i)->pid)
      break;
  list_insert(i, &stream, sizeof stream);
  return stream;
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
  ts_stream *stream;
  int e;

  while (1)
    {
      packet = ts_packets_read(packets);
      if (!packet)
        return 0;

      if (packet->pid == 0x1fff)
        continue;

      stream = ts_streams_lookup(streams, packet->pid);
      if (!stream)
        stream = ts_streams_create(streams, packet->pid, TS_STREAM_TYPE_UNKNOWN, 0);

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
