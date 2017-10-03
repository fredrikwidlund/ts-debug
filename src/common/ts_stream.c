#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/param.h>

#include <dynamic.h>

#include "ts_adaptation_field.h"
#include "ts_packet.h"
#include "ts_packets.h"
#include "ts_stream.h"

static void ts_stream_units_release(void *object)
{
  ts_unit *unit = *(ts_unit **) object;
  ts_unit_destruct(unit);
  free(unit);
}

void ts_unit_construct(ts_unit *unit)
{
  buffer_construct(&unit->data);
}

void ts_unit_destruct(ts_unit *unit)
{
  buffer_destruct(&unit->data);
}

void ts_unit_append(ts_unit *unit, void *data, size_t size)
{
  buffer_insert(&unit->data, buffer_size(&unit->data), data, size);
}

void ts_stream_construct(ts_stream *s, int pid)
{
  *s = (ts_stream) {.pid = pid};
  list_construct(&s->units);
}

void ts_stream_destruct(ts_stream *s)
{
  list_destruct(&s->units, ts_stream_units_release);
}

void ts_stream_type(ts_stream *s, int type, uint8_t id)
{
  s->type = type;
  s->id = id;
}

list *ts_stream_units(ts_stream *s)
{
  return &s->units;
}

ts_unit *ts_stream_read_unit(ts_stream *s)
{
  ts_unit *unit, **i;

  if (list_empty(ts_stream_units(s)))
    return NULL;
  i = list_front(ts_stream_units(s));
  unit = *i;
  list_erase(i, ts_stream_units_release);

  return unit;
}

ssize_t ts_stream_pack(ts_stream *s, ts_packets *packets)
{
  ts_unit **i;
  uint8_t *data;
  size_t size, n, cc, count;
  ts_packet *packet;

  count = 0;
  cc = 0;
  list_foreach(ts_stream_units(s), i)
    {
      data = buffer_data(&(*i)->data);
      size = buffer_size(&(*i)->data);
      n = 0;
      while (size)
        {
          packet = malloc(sizeof *packet);
          if (!packet)
            abort();
          ts_packet_construct(packet);
          packet->pid = s->pid;
          packet->continuity_counter = cc;
          if (n == 0)
            {
              packet->payload_unit_start_indicator = 1;
              if ((*i)->random_access_indicator)
                {
                  packet->adaptation_field_control |= 0x02;
                  packet->adaptation_field.random_access_indicator = 1;
                }
            }

          packet->payload_size = MIN(188 - ts_packet_size(packet), size);
          if (packet->payload_size)
            {
              packet->adaptation_field_control |= 0x01;
              packet->payload_data = malloc(packet->payload_size);
              memcpy(packet->payload_data, data, packet->payload_size);
              cc ++;
            }
          data += packet->payload_size;
          size -= packet->payload_size;
          list_push_back(ts_packets_list(packets), &packet, sizeof packet);
          n ++;
          count ++;
        }
    }

  return count;
}

ssize_t ts_stream_unpack(ts_stream *s, ts_packet *packet)
{
  ts_unit *unit;

  unit = list_empty(&s->units) ? NULL : *(ts_unit **) list_back(&s->units);
  if (packet->payload_unit_start_indicator)
    {
      unit = malloc(sizeof *unit);
      if (!unit)
        abort();
      ts_unit_construct(unit);
      if (packet->adaptation_field_control & 0x02 &&
          packet->adaptation_field.random_access_indicator)
        unit->random_access_indicator = 1;
      list_push_back(&s->units, &unit, sizeof unit);
    }

  if (!unit)
    return 0;

  ts_unit_append(unit, packet->payload_data, packet->payload_size);
  return 0;
}
