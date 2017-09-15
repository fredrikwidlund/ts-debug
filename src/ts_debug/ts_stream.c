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
#include "ts_stream.h"

/* ts_unit */

void ts_unit_destruct(ts_unit *unit)
{
  buffer_destruct(&unit->data);
}

ts_unit *ts_unit_new(int type)
{
  ts_unit *unit;

  unit = malloc(sizeof *unit);
  if (!unit)
    abort();
  unit->type = type;
  buffer_construct(&unit->data);

  return unit;
}

void ts_unit_type(ts_unit *unit, int type)
{
  unit->type = type;
}

int ts_unit_type_guess(ts_unit *unit)
{
  uint8_t *p;

  if (buffer_size(&unit->data) >= 3)
    {
      p = buffer_data(&unit->data);
      if (p[0] == 0x00 && p[1] == 0x00 && p[2] == 0x01)
        return TS_UNIT_TYPE_PES;

      return TS_UNIT_TYPE_PSI;
    }

  return TS_UNIT_TYPE_UNKNOWN;
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

ts_stream *ts_stream_new(int pid)
{
  ts_stream *stream;

  stream = malloc(sizeof *stream);
  if (!stream)
    abort();
  stream->pid = pid;
  stream->unit_type = TS_UNIT_TYPE_UNKNOWN;
  stream->continuity_counter = 0;
  list_construct(&stream->units);

  return stream;
}

void ts_stream_type(ts_stream *stream, int type)
{
  ts_unit **i;

  if (type == TS_UNIT_TYPE_UNKNOWN)
    return;

  stream->unit_type = type;
  list_foreach(&stream->units, i)
    ts_unit_type(*i, type);
}

int ts_stream_write(ts_stream *stream, ts_packet *packet)
{
  ts_unit **i, *unit;
  int e;

  if (packet->pid == 0x1fff)
    {
      ts_packet_delete(packet);
      return 0;
    }

  if (!list_empty(&stream->units) &&
      packet->adaptation_field_control & 0x01 &&
      ((stream->continuity_counter + 1) & 0x0f) != packet->continuity_counter)
    {
      ts_packet_delete(packet);
      return -1;
    }
  stream->continuity_counter = packet->continuity_counter;

  if (packet->payload_unit_start_indicator)
    {
      unit = ts_unit_new(stream->unit_type);
      list_push_back(&stream->units, &unit, sizeof unit);
    }
  else
    {
      i = list_back(&stream->units);
      if (i == list_end(&stream->units))
        {
          ts_packet_delete(packet);
          return 0;
        }
      unit = *i;
    }

  e = ts_unit_write(unit, packet);
  if (e == -1)
    return -1;

  if (stream->unit_type == TS_UNIT_TYPE_UNKNOWN)
    ts_stream_type(stream, ts_unit_type_guess(unit));

  return 0;
}

void ts_stream_debug(ts_stream *stream, FILE *f, int indent)
{
  ts_unit **i;
  int n;

  (void) fprintf(f, "%*s[pid %d, type %d]\n", indent * 2, "", stream->pid, stream->unit_type);
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
          stream = ts_stream_new(packet->pid);
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
