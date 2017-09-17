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
#include "ts_psi.h"
#include "ts_stream.h"

int main()
{
  ts_packets packets;
  char block[1048476];
  ssize_t n;
  ts_streams streams;
  int e;

  ts_packets_construct(&packets);

  while (1)
    {
      n = read(0, block, sizeof block);
      if (n == -1)
        err(1, "read");

      if (!n)
        break;

      n = ts_packets_write(&packets, block, n);
      if (n == -1)
        errx(1, "ts_write");
    }

  ts_streams_construct(&streams);
  e = ts_streams_write(&streams, &packets);
  if (e == -1)
    errx(1, "ts_streams_write");
  ts_packets_destruct(&packets);

  ts_streams_debug(&streams, stdout, 0);
  ts_streams_destruct(&streams);
}
