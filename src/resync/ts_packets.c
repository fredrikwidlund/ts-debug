#include <stdlib.h>
#include <stdint.h>

#include <dynamic.h>

#include "ts_adaptation_field.h"
#include "ts_packet.h"
#include "ts_packets.h"

ssize_t ts_packets_pack(list *packets, stream *stream)
{
  ts_packet **i;
  ssize_t n, count;
  count = 0;
  list_foreach(packets, i)
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

ssize_t ts_packets_unpack(list *packets, stream *stream)
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
      list_push_back(packets, &packet, sizeof packet);
      count += n;
    }

  return count;
}
