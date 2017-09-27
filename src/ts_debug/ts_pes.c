#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <err.h>

#include <dynamic.h>

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
  stream s;
  uint32_t v;
  uint64_t t;
  int len;

  stream_construct(&s, data, size);
  v = stream_read32(&s);
  if (stream_read_bits(v, 32, 0, 24) != 0x000001)
    {
      stream_destruct(&s);
      return -1;
    }
  pes->id = stream_read_bits(v, 32, 24, 8);
  len = stream_read16(&s);
  if (len && (size_t) len != stream_size(&s))
    {
      stream_destruct(&s);
      return -1;
    }

  if (pes->id != 0xbe && pes->id != 0xbf)
    {
      v = stream_read16(&s);
      if (stream_read_bits(v, 16, 0, 2) != 0x02)
        {
          stream_destruct(&s);
          return -1;
        }
      pes->scrambling_control = stream_read_bits(v, 16, 2, 2);
      pes->priority = stream_read_bits(v, 16, 4, 1);
      pes->data_alignment_indicator = stream_read_bits(v, 16, 5, 1);
      pes->copyright = stream_read_bits(v, 16, 6, 1);
      pes->original = stream_read_bits(v, 16, 7, 1);
      pes->has_pts = stream_read_bits(v, 16, 8, 1);
      pes->has_dts = stream_read_bits(v, 16, 9, 1);
      len = stream_read8(&s);
      if (pes->has_pts)
        {
          if (len < 5)
            {
              stream_destruct(&s);
              return -1;
            }
          t = stream_read32(&s);
          t <<= 8;
          t += stream_read8(&s);
          len -= 5;
          pes->pts = (stream_read_bits(t, 40, 4, 3) << 30) + (stream_read_bits(t, 40, 8, 15) << 15) + stream_read_bits(t, 40, 24, 15);
        }
      if (pes->has_dts)
        {
          if (len < 5)
            {
              stream_destruct(&s);
              return -1;
            }
          t = stream_read32(&s);
          t <<= 8;
          t += stream_read8(&s);
          len -= 5;
          pes->dts = (stream_read_bits(t, 40, 4, 3) << 30) + (stream_read_bits(t, 40, 8, 15) << 15) + stream_read_bits(t, 40, 24, 15);
        }
      stream_read(&s, NULL, len);
    }

  if (!stream_valid(&s))
    {
      stream_destruct(&s);
      return -1;
    }

  pes->payload_size = stream_size(&s);
  pes->payload_data = malloc(pes->payload_size);
  if (!pes->payload_data)
    abort();
  memcpy(pes->payload_data, stream_data(&s), pes->payload_size);

  stream_destruct(&s);
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
