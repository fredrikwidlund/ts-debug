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
/*
#include "ts_psi.h"
#include "ts_pes.h"
#include "ts_stream.h"
#include "ts_streams.h"
*/

int main()
{
  ts_packets packets;
  char block[1048476];
  //char *p;
  //void *data;
  //size_t size;
  ssize_t n, e;
  //ts_streams streams;

  ts_packets_construct(&packets);

  while (1)
    {
      n = read(0, block, sizeof block);
      if (n == -1)
        err(1, "read");

      if (!n)
        break;

      e = ts_packets_unpack(&packets, block, n);
      if (e == -1)
        errx(1, "ts_packets_unpack");
    }

  /*
  ts_streams_construct(&streams);
  e = ts_streams_write(&streams, &packets);
  if (e == 0)
    e = ts_streams_write(&streams, NULL);
  if (e == -1)
    errx(1, "ts_streams_write");
  */

  ts_packets_destruct(&packets);

  //ts_streams_debug(&streams, stdout, 0);

  /*
  ts_packets_construct(&packets);
  e = ts_streams_pack(&streams, &packets);
  if (e == -1)
    err(1, "ts_streams_pack");
  ts_streams_destruct(&streams);

  e = ts_packets_pack(&packets, &data, &size);
  if (e == -1)
    err(1, "ts_packets_pack");
  ts_packets_destruct(&packets);

  p = data;
  while (size)
    {
      n = write(1, p, size);
      if (n == -1)
        err(1, "write");
      size -= n;
      p += n;
    }
  free(data);
  */
}
