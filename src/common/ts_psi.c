#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/param.h>

#include <dynamic.h>

#include "ts_psi.h"

void ts_psi_construct(ts_psi *psi)
{
  *psi = (ts_psi) {0};
}

void ts_psi_destruct(ts_psi *psi)
{
  *psi = (ts_psi) {0};
}

ssize_t ts_psi_unpack_stream(ts_psi *psi, stream *stream)
{
  int v;

  psi->id = stream_read8(stream);
  if (psi->id == 0xff)
    return 1;

  v = stream_read16(stream);
  if (!stream_valid(stream))
    return 0;

  if (stream_read_bits(v, 16, 0, 1) != 0x01 ||
      stream_read_bits(v, 16, 1, 1) != 0x00 ||
      stream_read_bits(v, 16, 2, 2) != 0x03 ||
      stream_read_bits(v, 16, 4, 2) != 0x00)
    return -1;
  psi->section_length = stream_read_bits(v, 16, 6, 10);
  if (psi->section_length < 9)
    return -1;
  if (stream_size(stream) < psi->section_length)
    return 0;

  psi->id_extension = stream_read16(stream);

  v = stream_read8(stream);
  if (stream_read_bits(v, 8, 0, 2) != 0x03)
    return -1;
  psi->version = stream_read_bits(v, 8, 2, 5);
  psi->current = stream_read_bits(v, 8, 7, 1);
  psi->section_number = stream_read8(stream);
  psi->last_section_number = stream_read8(stream);

  return stream_valid(stream) ? 1 : 0;
}

ssize_t ts_psi_pointer_unpack(stream *stream)
{
  uint8_t size;

  size = stream_read8(stream);
  stream_read(stream, NULL, size);
  return stream_valid(stream) ? 1 : 0;
}

/*
ssize_t ts_psi_unpack_stream(ts_psi *psi, stream *stream)
{
  ssize_t n;
  ts_psi_table table;

  n = ts_psi_pointer_unpack(stream);
  if (n <= 0)
    return n;

  while (1)
    {
      ts_psi_table_construct(&table);
      n = ts_psi_table_unpack(&table, stream);
      if (n <= 0)
        {
          ts_psi_table_destruct(&table);
          return n;
        }
      if (table.id == 0xff)
        break;

      fprintf(stderr, "id %u/%u version %u current %u (%u/%u)\n", table.id, table.id_extension,
              table.version, table.current,
              table.section_number, table.last_section_number);
      if (table.section_number == table.last_section_number)
        break;
    }

  return 1;
}
*/
