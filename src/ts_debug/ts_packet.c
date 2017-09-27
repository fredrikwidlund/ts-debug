#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <err.h>

#include <dynamic.h>

#include "ts_packet.h"

/* internals */

static uint64_t stream_read48(stream *stream)
{
  return ((uint64_t) stream_read32(stream) << 16) + stream_read16(stream);
}

static ssize_t ts_adaptation_field_unpack(ts_adaptation_field *af, void *data, size_t size)
{
  stream s;
  int len, valid;
  uint8_t flags;
  uint64_t v;

  *af = (ts_adaptation_field){0};
  stream_construct(&s, data, size);
  len = stream_read8(&s);
  if (len)
    {
      flags = stream_read8(&s);
      af->discontinuity_indicator = stream_read_bits(flags, 8, 0, 1);
      af->random_access_indicator = stream_read_bits(flags, 8, 1, 1);
      af->elementary_stream_priority_indicator = stream_read_bits(flags, 8, 2, 1);
      af->pcr_flag = stream_read_bits(flags, 8, 3, 1);
      af->opcr_flag = stream_read_bits(flags, 8, 4, 1);
      af->splicing_point_flag = stream_read_bits(flags, 8, 5, 1);
      af->transport_private_data_flag = stream_read_bits(flags, 8, 6, 1);
      af->adaptation_field_extension_flag = stream_read_bits(flags, 8, 7, 1);
      if (af->pcr_flag)
        {
          v = stream_read48(&s);
          af->pcr = 300 * (v >> 15) + (v & 0x1ff);
        }
    }

  valid = stream_valid(&s);
  stream_destruct(&s);
  return valid ? len + 1 : -1;
}

/* packet */

void ts_packet_construct(ts_packet *packet)
{
  *packet = (ts_packet) {0};
}

void ts_packet_destruct(ts_packet *packet)
{
  free(packet->data);
  ts_packet_construct(packet);
}

/*
ts_packet *ts_packet_new(void)
{
  ts_packet *packet;

  packet = malloc(sizeof *packet);
  if (!packet)
    abort();
  ts_packet_construct(packet);
  return packet;
}

void ts_packet_delete(ts_packet *packet)
{
  ts_packet_destruct(packet);
  free(packet);
}
*/

ssize_t ts_packet_unpack(ts_packet *packet, void *data, size_t size)
{
  stream s;
  uint32_t v;
  ssize_t n;
  int valid;

  if (size != 188)
    return -1;

  stream_construct(&s, data, size);
  v = stream_read32(&s);
  packet->transport_error_indicator = stream_read_bits(v, 32, 8, 1);
  packet->payload_unit_start_indicator = stream_read_bits(v, 32, 9, 1);
  packet->transport_priority = stream_read_bits(v, 32, 10, 1);
  packet->pid = stream_read_bits(v, 32, 11, 13);
  packet->transport_scrambling_control = stream_read_bits(v, 32, 24, 2);
  packet->adaptation_field_control = stream_read_bits(v, 32, 26, 2);
  packet->continuity_counter = stream_read_bits(v, 32, 28, 4);

  if (packet->adaptation_field_control & 0x02)
    {
      n = ts_adaptation_field_unpack(&packet->adaptation_field, stream_data(&s), stream_size(&s));
      if (n <= 0)
        {
          stream_destruct(&s);
          return -1;
        }
      stream_read(&s, NULL, n);
    }

  if (packet->adaptation_field_control & 0x01)
    {
      packet->size = stream_size(&s);
      packet->data = malloc(packet->size);
      if (!packet->data)
        abort();
      memcpy(packet->data, stream_data(&s), packet->size);
    }

  valid = stream_valid(&s);
  stream_destruct(&s);
  return valid ? 188 : -1;
}
