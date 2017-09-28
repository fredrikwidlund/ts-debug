#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <err.h>

#include <dynamic.h>

#include "ts_packet.h"
#include "ts_packets.h"
#include "ts_psi.h"
#include "ts_pes.h"
#include "ts_stream.h"
#include "ts_streams.h"

/* internals */

/* ts_unit */

void ts_unit_destruct(ts_unit *unit)
{
  buffer_destruct(&unit->data);
}

void ts_unit_construct(ts_unit *unit)
{
  unit->complete = 0;
  ts_pes_construct(&unit->pes);
  buffer_construct(&unit->data);
}

ts_unit *ts_unit_new(void)
{
  ts_unit *unit;

  unit = malloc(sizeof *unit);
  if (!unit)
    abort();
  ts_unit_construct(unit);
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

void ts_unit_write(ts_unit *unit, void *data, size_t size)
{
  buffer_insert(&unit->data, buffer_size(&unit->data), data, size);
}

ssize_t ts_unit_unpack(ts_unit *unit, ts_packet *packet)
{
  ts_unit_write(unit, packet->data, packet->size);
  return packet->size;
}

ssize_t ts_unit_pack(ts_unit *unit, ts_packets *packets, int pid, size_t cc)
{
  ts_packet *packet;
  uint8_t *data;
  size_t size;
  ssize_t n;

  data = buffer_data(&unit->data);
  size = buffer_size(&unit->data);

  n = 0;
  while (size)
    {
      packet = malloc(sizeof *packet);
      if (!packet)
        abort();
      ts_packet_construct(packet);
      packet->size = 188;
      packet->pid = pid;
      packet->adaptation_field_control = 0x01;
      packet->continuity_counter = cc + n;
      packet->size -= 4;

      if (n == 0)
        {
          packet->payload_unit_start_indicator = 1;
          packet->adaptation_field_control |= 0x02;
          packet->adaptation_field.random_access_indicator = 1;
          packet->size -= 2;
        }

      if (size < packet->size)
        packet->size = size;

      packet->data = malloc(packet->size);
      if (!packet->data)
        abort();
      memcpy(packet->data, data, packet->size);

      data += packet->size;
      size -= packet->size;

      list_push_back(&packets->list, &packet, sizeof packet);
      n ++;
    }

  return n;
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
  ts_unit *unit = *(ts_unit **) object;

  ts_unit_destruct(unit);
  ts_pes_destruct(&unit->pes);
  free(unit);
}

static int ts_stream_process(ts_stream *stream, ts_unit *unit)
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

static int ts_stream_complete(ts_stream *stream)
{
  ts_unit *unit;

  if (list_empty(&stream->units))
    return 0;

  unit = *(ts_unit **) list_back(&stream->units);
  if (unit->complete)
    return 0;

  unit->complete = 1;
  return ts_stream_process(stream, unit);
}

/* ts_stream */

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

ssize_t ts_stream_unpack(ts_stream *stream, ts_packet *packet)
{
  ts_unit *unit;
  ssize_t n;
  int e, empty;

  if (!packet)
    return ts_stream_complete(stream);

  // ignore NULL packets
  if (packet->pid == 0x1fff)
    return 0;

  empty = list_empty(&stream->units);
  // validate and set continuity counters
  if (!empty && packet->adaptation_field_control & 0x01 &&
      ((stream->continuity_counter + 1) & 0x0f) != packet->continuity_counter)
    return -1;

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
    return 0;

  // write packet to stream
  n = ts_unit_unpack(unit, packet);
  if (n == -1)
    return -1;

  return ts_stream_process(stream, unit);
}

ssize_t ts_stream_pack(ts_stream *stream, ts_packets *packets)
{
  void *data;
  size_t size;
  ssize_t n, cc;
  ts_unit *unit, **i;

  switch (stream->type)
    {
    case TS_STREAM_TYPE_PSI:
      // unit data save to buffer
      list_clear(&stream->units, ts_stream_units_release);
      n = ts_psi_pack(&stream->streams->psi, &data, &size, stream->content_type);
      if (n == -1)
        return -1;

      unit = ts_unit_new();
      ts_unit_write(unit, data, size);
      free(data);
      list_push_back(&stream->units, &unit, sizeof unit);

      // pack buffer
      cc = 0;
      list_foreach(&stream->units, i)
        {
          n = ts_unit_pack(*i, packets, stream->pid, cc);
          if (n == -1)
            return -1;
          cc += n;
        }
      return cc;
    default:
      return 0;
    }
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
