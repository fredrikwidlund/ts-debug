#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <err.h>

#include <dynamic.h>

#include "bytes.h"
#include "ts_pes.h"

void ts_pes_construct(ts_pes *pes)
{
  *pes = (ts_pes) {0};
}

void ts_pes_destruct(ts_pes *pes)
{
  (void) pes;
}

ssize_t ts_pes_unpack(ts_pes *pes, void *data, size_t size)
{
  bytes b;
  uint32_t v;

  bytes_construct(&b, data, size);
  v = bytes_read4(&b);
  if (bytes_bits(v, 32, 0, 24) != 0x000001)
    return -1;
  pes->id = bytes_bits(v, 32, 24, 8);
  printf("%02x\n", pes->id);
  return size;
}
