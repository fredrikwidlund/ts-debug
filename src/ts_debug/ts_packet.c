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

/* ts_adaptation_field */

static ssize_t ts_adaptation_field_parse(ts_adaptation_field *af, void *data, size_t size)
{
  bytes b;
  int len;

  bytes_construct(&b, data, size);
  len = bytes_read1(&b);
  (void) af;

  return len + 1;
}

/* packet */

static ssize_t ts_packet_parse(ts_packet *packet, void *data, size_t size)
{
  bytes b;
  uint32_t v;
  ssize_t n;

  if (size != 188)
    return -1;

  packet->size = size;
  packet->data = malloc(packet->size);
  if (!packet->data)
    abort();
  memcpy(packet->data, data, packet->size);

  bytes_construct(&b, packet->data, packet->size);
  v = bytes_read4(&b);
  packet->transport_error_indicator = bytes_bits(v, 32, 8, 1);
  packet->payload_unit_start_indicator = bytes_bits(v, 32, 9, 1);
  packet->transport_priority = bytes_bits(v, 32, 10, 1);
  packet->pid = bytes_bits(v, 32, 11, 13);
  packet->transport_scrambling_control = bytes_bits(v, 32, 24, 2);
  packet->adaptation_field_control = bytes_bits(v, 32, 26, 2);
  packet->continuity_counter = bytes_bits(v, 32, 28, 4);

  if (packet->adaptation_field_control & 0x02)
    {
      n = ts_adaptation_field_parse(&packet->adaptation_field, bytes_data(&b), bytes_size(&b));
      if (n == -1)
        return -1;
      bytes_read(&b, NULL, n);
    }

  if (packet->adaptation_field_control & 0x01)
    bytes_rest(&b, &packet->payload_data, &packet->payload_size);

  return (bytes_valid(&b) && !bytes_size(&b)) ? (ssize_t) size : -1;
}

void ts_packet_destruct(ts_packet *packet)
{
  free(packet->data);
}

ts_packet *ts_packet_new(void *data, size_t size)
{
  ts_packet *packet;
  int e;

  packet = calloc(1, sizeof *packet);
  if (!packet)
    abort();

  e = ts_packet_parse(packet, data, size);
  if (e == -1)
    {
      ts_packet_delete(packet);
      return NULL;
    }

  return packet;
}

void ts_packet_delete(ts_packet *packet)
{
  ts_packet_destruct(packet);
  free(packet);
}

/* packets */

static void ts_packets_packets_release(void *object)
{
  ts_packet_delete(*(ts_packet **) object);
}

void ts_packets_construct(ts_packets *packets)
{
  buffer_construct(&packets->in);
  list_construct(&packets->packets);
}

void ts_packets_destruct(ts_packets *packets)
{
  list_destruct(&packets->packets, ts_packets_packets_release);
}

int ts_packets_write(ts_packets *packets, void *data, size_t size)
{
  ts_packet *packet;
  uint8_t *b;
  size_t n;

  buffer_insert(&packets->in, buffer_size(&packets->in), data, size);

  for (b = buffer_data(&packets->in), n = buffer_size(&packets->in); n >= 188; b += 188, n -= 188)
    {
      packet = ts_packet_new(b, 188);
      if (!packet)
        return -1;

      list_push_back(&packets->packets, &packet, sizeof packet);
    }

  buffer_erase(&packets->in, 0, buffer_size(&packets->in) - n);
  return 0;
}

ts_packet *ts_packets_read(ts_packets *packets)
{
  ts_packet **i, *packet;

  i = list_front(&packets->packets);
  if (i == list_end(&packets->packets))
    return NULL;

  packet = *i;
  list_erase(i, NULL);

  return packet;
}
