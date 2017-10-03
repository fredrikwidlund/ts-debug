#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#include <dynamic.h>

#include "ts_adaptation_field.h"

static ssize_t ts_pcr_pack(uint64_t *value, stream *stream)
{
  uint64_t base, ext, v;

  base = *value / 300;
  ext = *value - (base * 300);
  v = (base << 15) + (ext & 0x1ff);
  stream_write32(stream, v >> 16);
  stream_write16(stream, v);

  return stream_valid(stream) ? 1 : -1;
}

static ssize_t ts_pcr_unpack(uint64_t *value, stream *stream)
{
  uint64_t v;

  v = ((uint64_t) stream_read32(stream) << 16) + stream_read16(stream);
  *value = (stream_read_bits(v, 48, 0, 33) * 300) + stream_read_bits(v, 48, 39, 9);

  return stream_valid(stream) ? 1 : -1;
}

void ts_adaptation_field_construct(ts_adaptation_field *af)
{
  *af = (ts_adaptation_field) {0};
}

void ts_adaptation_field_destruct(ts_adaptation_field *af)
{
  free(af->private_data);
  ts_adaptation_field_construct(af);
}

size_t ts_adaptation_field_size(ts_adaptation_field *af)
{
  return 2 + (af->pcr_flag ? 6 : 0) + (af->opcr_flag ? 6 : 0) + (af->splicing_point_flag ? 1 : 0) +
    (af->transport_private_data_flag ? af->private_size + 1 : 0);
}

ssize_t ts_adaptation_field_pack(ts_adaptation_field *af, stream *stream, size_t size)
{
  stream_write8(stream, size - 1);
  if (size == 1)
    return stream_valid(stream) ? 1 : -1;

  stream_write8(stream,
                stream_write_bits(af->discontinuity_indicator, 8, 0, 1) |
                stream_write_bits(af->random_access_indicator, 8, 1, 1) |
                stream_write_bits(af->elementary_stream_priority_indicator, 8, 2, 1) |
                stream_write_bits(af->pcr_flag, 8, 3, 1) |
                stream_write_bits(af->opcr_flag, 8, 4, 1) |
                stream_write_bits(af->splicing_point_flag, 8, 5, 1) |
                stream_write_bits(af->transport_private_data_flag, 8, 6, 1) |
                stream_write_bits(af->adaptation_field_extension_flag, 8, 7, 1));

  if (af->pcr_flag)
    ts_pcr_pack(&af->pcr, stream);

  if (af->opcr_flag)
    ts_pcr_pack(&af->opcr, stream);

  if (af->splicing_point_flag)
    stream_write8(stream, af->splice_countdown);

  if (af->private_size)
    {
      stream_write8(stream, af->private_size);
      stream_write(stream, af->private_data, af->private_size);
    }

  size -= ts_adaptation_field_size(af);
  while (size)
    {
      stream_write8(stream, 0xff);
      size --;
    }

  return stream_valid(stream) ? 1 : -1;
}

ssize_t ts_adaptation_field_unpack(ts_adaptation_field *af, stream *stream)
{
  size_t size;
  uint8_t len, flags;

  len = stream_read8(stream);
  if (len)
    {
      size = stream_size(stream);
      flags = stream_read8(stream);
      af->discontinuity_indicator = stream_read_bits(flags, 8, 0, 1);
      af->random_access_indicator = stream_read_bits(flags, 8, 1, 1);
      af->elementary_stream_priority_indicator = stream_read_bits(flags, 8, 2, 1);
      af->pcr_flag = stream_read_bits(flags, 8, 3, 1);
      af->opcr_flag = stream_read_bits(flags, 8, 4, 1);
      af->splicing_point_flag = stream_read_bits(flags, 8, 5, 1);
      af->transport_private_data_flag = stream_read_bits(flags, 8, 6, 1);
      af->adaptation_field_extension_flag = stream_read_bits(flags, 8, 7, 1);

      if (af->pcr_flag)
        ts_pcr_unpack(&af->pcr, stream);

      if (af->opcr_flag)
        ts_pcr_unpack(&af->opcr, stream);

      if (af->splicing_point_flag)
        af->splice_countdown = stream_read8(stream);

      if (af->transport_private_data_flag)
        {
          af->private_size = stream_read8(stream);
          af->private_data = malloc(af->private_size);
          if (!af->private_data)
            abort();
          stream_read(stream, af->private_data, af->private_size);
        }

      // extensions not supported yet
      af->adaptation_field_extension_flag = 0;

      stream_read(stream, NULL, len - (size - stream_size(stream)));
    }

  return stream_valid(stream) ? 1 : -1;
}
