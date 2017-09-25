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

/* internals */

static void ts_packets_packets_release(void *object)
{
  ts_packet *p = *(ts_packet **) object;

  ts_packet_destruct(p);
  free(p);
}

/* ts_packets */

void ts_packets_construct(ts_packets *packets)
{
  buffer_construct(&packets->in);
  list_construct(&packets->packets);
}

void ts_packets_destruct(ts_packets *packets)
{
  list_destruct(&packets->packets, ts_packets_packets_release);
  buffer_destruct(&packets->in);
}

ssize_t ts_packets_unpack(ts_packets *packets, void *data, size_t size)
{
  ts_packet *packet;
  uint8_t *begin, *end;
  ssize_t e, n;

  buffer_insert(&packets->in, buffer_size(&packets->in), data, size);
  for (begin = buffer_data(&packets->in), end = begin + buffer_size(&packets->in), n = 0; end - begin >= 188; begin += 188, n ++)
    {
      packet = malloc(sizeof *packet);
      if (!packet)
        abort();
      ts_packet_construct(packet);
      e = ts_packet_unpack(packet, begin, 188);
      if (e <= 0)
        return -1;

      list_push_back(&packets->packets, &packet, sizeof packet);
    }

  buffer_erase(&packets->in, 0, buffer_size(&packets->in) - (end - begin));
  return n;
}

ssize_t ts_packets_pack(ts_packets *packets, void **data, size_t *size)
{
  (void) packets;
  *data = NULL;
  *size = 0;

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
