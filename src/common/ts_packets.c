#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <dynamic.h>

#include "ts_adaptation_field.h"
#include "ts_packet.h"
#include "ts_packets.h"

static void ts_packets_list_release(void *object)
{
  ts_packet *p = *(ts_packet **) object;

  ts_packet_destruct(p);
}

void ts_packets_construct(ts_packets *packets)
{
  list_construct(&packets->list);
}

void ts_packets_destruct(ts_packets *packets)
{
  list_destruct(&packets->list, ts_packets_list_release);
}

list *ts_packets_list(ts_packets *packets)
{
  return &packets->list;
}

ssize_t ts_packets_pack(ts_packets *packets, stream *stream)
{
  ts_packet **i;
  ssize_t n, count;

  count = 0;
  list_foreach(ts_packets_list(packets), i)
    {
      n = ts_packet_pack(*i, stream);
      if (n == -1)
        return -1;
      if (n == 0)
        break;
      count += n;
    }

  return count;
}

ssize_t ts_packets_unpack(ts_packets *packets, stream *stream)
{
  ts_packet *packet;
  ssize_t n, count;

  count = 0;
  while (stream_size(stream))
    {
      packet = malloc(sizeof *packet);
      if (!packet)
        abort();
      ts_packet_construct(packet);
      n = ts_packet_unpack(packet, stream);
      if (n <= 0)
        {
          ts_packet_destruct(packet);
          return n == -1 ? -1 : count;
        }
      list_push_back(ts_packets_list(packets), &packet, sizeof packet);
      count += n;
    }

  return count;
}
