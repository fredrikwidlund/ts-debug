#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <dynamic.h>

#include "ts_pes.h"

ssize_t ts_pes_ts_unpack(uint64_t *ts, stream *s)
{
  (void) s;
  *ts = 0;
  return 1;
}

void ts_pes_construct(ts_pes *pes)
{
  *pes = (ts_pes) {0};
}

ssize_t ts_pes_construct_buffer(ts_pes *pes, buffer *buffer)
{
  ssize_t n;

  ts_pes_construct(pes);
  n = ts_pes_unpack_buffer(pes, buffer);
  if (n <= 0)
    ts_pes_destruct(pes);
  return n;
}

void ts_pes_destruct(ts_pes *pes)
{
  *pes = (ts_pes) {0};
}

ssize_t ts_pes_unpack_stream(ts_pes *pes, stream *s)
{
  int v;
  size_t len;
  ssize_t n;

  v = stream_read32(s);
  if (stream_read_bits(v, 32, 0, 24) != 0x000001)
    return -1;
  pes->stream_id = stream_read_bits(v, 32, 24, 8);
  len = stream_read16(s);
  if (((pes->stream_id < 0xe0 || pes->stream_id >= 0xf0) && len == 0) ||
      (len && len != stream_size(s)))
    return -1;

  if (pes->stream_id != 0xbe && pes->stream_id != 0xbf)
    {
      v = stream_read16(s);
      if (stream_read_bits(v, 16, 0, 2) != 0x02)
        return -1;

      pes->scrambling_control = stream_read_bits(v, 16, 2, 2);
      pes->priority = stream_read_bits(v, 16, 4, 1);
      pes->data_alignment_indicator = stream_read_bits(v, 16, 5, 1);
      pes->copyright = stream_read_bits(v, 16, 6, 1);
      pes->original_or_copy = stream_read_bits(v, 16, 7, 1);
      pes->pts_indicator = stream_read_bits(v, 16, 8, 1);
      pes->dts_indicator = stream_read_bits(v, 16, 9, 1);
      len = stream_read8(s);
      if (pes->pts_indicator)
        {
          n = ts_pes_ts_unpack(&pes->pts, s);
          if (n == -1 || len < 5 || !stream_valid(s))
            return -1;
          len -= 5;
        }
      if (pes->dts_indicator)
        {
          n = ts_pes_ts_unpack(&pes->dts, s);
          if (n == -1 || len < 5 || !stream_valid(s))
            return -1;
          len -= 5;
        }

      stream_read(s, NULL, len);
    }

  if (!stream_valid(s))
    return -1;

  pes->size = stream_size(s);
  pes->data = malloc(pes->size);
  if (!pes->data)
    abort();
  memcpy(pes->data, stream_data(s), pes->size);

  return 1;
}

ssize_t ts_pes_unpack_buffer(ts_pes *pes, buffer *buffer)
{
  stream stream;
  ssize_t n;

  stream_construct_buffer(&stream, buffer);
  n = ts_pes_unpack_stream(pes, &stream);
  stream_destruct(&stream);

  return n;
}

void ts_pes_debug(ts_pes *pes, FILE *f)
{
  (void) fprintf(f, "[pes] stream id 0x%02x, size %lu\n",
                 pes->stream_id, pes->size);
}
