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
  free(pes->payload_data);
  *pes = (ts_pes) {0};
}

ssize_t ts_pes_unpack(ts_pes *pes, void *data, size_t size)
{
  bytes b;
  uint32_t v;
  uint64_t t;
  int len;

  bytes_construct(&b, data, size);
  v = bytes_read4(&b);
  if (bytes_bits(v, 32, 0, 24) != 0x000001)
    return -1;
  pes->id = bytes_bits(v, 32, 24, 8);
  len = bytes_read2(&b);
  if (len && (size_t) len != bytes_size(&b))
    return -1;

  if (pes->id != 0xbe && pes->id != 0xbf)
    {
      v = bytes_read2(&b);
      if (bytes_bits(v, 16, 0, 2) != 0x02)
        return -1;
      pes->scrambling_control = bytes_bits(v, 16, 2, 2);
      pes->priority = bytes_bits(v, 16, 4, 1);
      pes->data_alignment_indicator = bytes_bits(v, 16, 5, 1);
      pes->copyright = bytes_bits(v, 16, 6, 1);
      pes->original = bytes_bits(v, 16, 7, 1);
      pes->has_pts = bytes_bits(v, 16, 8, 1);
      pes->has_dts = bytes_bits(v, 16, 9, 1);
      len = bytes_read1(&b);
      if (pes->has_pts)
        {
          t = bytes_read5(&b);
          if (len < 5)
            return -1;
          len -= 5;
          pes->pts = (bytes_bits(t, 40, 4, 3) << 30) + (bytes_bits(t, 40, 8, 15) << 15) + bytes_bits(t, 40, 24, 15);
        }
      if (pes->has_dts)
        {
          t = bytes_read5(&b);
          if (len < 5)
            return -1;
          len -= 5;
          pes->dts = (bytes_bits(t, 40, 4, 3) << 30) + (bytes_bits(t, 40, 8, 15) << 15) + bytes_bits(t, 40, 24, 15);
        }
      bytes_read(&b, NULL, len);
    }

  if (!bytes_valid(&b))
    return -1;

  pes->payload_size = bytes_size(&b);
  pes->payload_data = malloc(pes->payload_size);
  if (!pes->payload_data)
    abort();
  memcpy(pes->payload_data, bytes_data(&b), pes->payload_size);

  return size;
}

void ts_pes_debug(ts_pes *pes, FILE *f, int indent)
{
  uint8_t *data = pes->payload_data;
  size_t i;

  (void) fprintf(f, "%*s[id %02x", indent, "", pes->id);
  if (pes->has_pts)
    (void) fprintf(f, ", pts %lu", pes->pts);
  if (pes->has_dts)
    (void) fprintf(f, ", dts %lu", pes->dts);
  (void) fprintf(f, ", size %lu, data ", pes->payload_size);

  for (i = 0; i < 16 && i < pes->payload_size; i ++)
    (void) fprintf(f, "%02x", data[i]);
  (void) fprintf(f, "]\n");
}
