#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <err.h>
#include <fcntl.h>

#include <dynamic.h>

#include "ts_packet.h"
#include "ts_packets.h"
#include "ts_psi.h"
#include "ts_pes.h"
#include "ts_stream.h"
#include "ts_streams.h"

int main()
{
  ts_packets packets;
  char block[1048476];
  ssize_t n, e;
  ts_streams streams;

  ts_packets_construct(&packets);

  // read ts packets
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

  // demux stream
  ts_streams_construct(&streams);
  e = ts_streams_unpack(&streams, &packets);
  if (e != -1)
    e = ts_streams_unpack(&streams, NULL);
  if (e == -1)
    errx(1, "ts_streams_write");
  ts_packets_destruct(&packets);

  // mux streams
  ts_streams_debug(&streams, stdout, 0);

  ts_packets_construct(&packets);
  n = ts_streams_pack(&streams, &packets);
  ts_streams_destruct(&streams);

  ts_packets_destruct(&packets);

  // write ts packets
}
