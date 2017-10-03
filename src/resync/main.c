#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <err.h>
#include <dynamic.h>

#include "ts_adaptation_field.h"
#include "ts_packet.h"
#include "ts_packets.h"

int main()
{
  buffer buffer;
  stream stream;
  ts_packets packets;
  char data[1048476], *p;
  ssize_t n, size;
  ts_packet **i;

  // read input
  buffer_construct(&buffer);
  while (1)
    {
      size = read(0, data, sizeof data);
      if (size == -1)
        err(1, "read");
      if (!size)
        break;
      buffer_insert(&buffer, buffer_size(&buffer), data, size);
    }

  // unpack packets
  ts_packets_construct(&packets);
  stream_construct_buffer(&stream, &buffer);
  n = ts_packets_unpack(&packets, &stream);
  if (n == -1)
    err(1, "ts_packets_unpack");
  stream_destruct(&stream);
  buffer_destruct(&buffer);

  // do something with the packets
  list_foreach(ts_packets_list(&packets), i)
    (*i)->adaptation_field.pcr_flag = 0;

  // pack packets
  buffer_construct(&buffer);
  stream_construct_buffer(&stream, &buffer);
  n = ts_packets_pack(&packets, &stream);
  if (n == -1)
    err(1, "ts_packets_pack");
  ts_packets_destruct(&packets);
  stream_destruct(&stream);

  // write output
  p = buffer_data(&buffer);
  size = buffer_size(&buffer);
  while (size)
    {
      n = write(1, p, size);
      if (n == -1)
        err(1, "write");
      p += n;
      size -= n;
    }
  buffer_destruct(&buffer);
}
