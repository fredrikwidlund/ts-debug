#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <dynamic.h>

#include "ts_psi.h"
#include "ts_pat.h"

void ts_pat_construct(ts_pat *pat)
{
  *pat = (ts_pat) {0};
}

void ts_pat_destruct(ts_pat *pat)
{
  *pat = (ts_pat) {0};
}

/*
 while (stream_size(&s))
    {
      if (pat->present)
        return -1;
      v = stream_read32(&s);
      if (stream_read_bits(v, 32, 16, 3) != 0x07)
        {
          stream_destruct(&s);
          return -1;
        }
      num = stream_read_bits(v, 32, 0, 16);
      pid = stream_read_bits(v, 32, 19, 13);

      pat->present = 1;
      pat->id_extension = id_extension;
      pat->version = version;
      pat->program_number = num;
      pat->program_pid = pid;
    }
*/

ssize_t ts_pat_unpack_stream(ts_pat *pat, stream *data)
{
  ts_psi psi;
  ssize_t n;
  stream table;
  int v;

  n = ts_psi_pointer_unpack(data);
  if (n <= 0)
    return n;

  ts_psi_construct(&psi);
  n = ts_psi_unpack_stream(&psi, data);
  if (n <= 0)
    {
      ts_psi_destruct(&psi);
      return n;
    }

  if (stream_size(data) < (size_t) (psi.section_length - 5))
    {
      ts_psi_destruct(&psi);
      return -1;
    }

  stream_construct(&table, stream_data(data), psi.section_length - 9);
  while (stream_size(&table))
    {
      v = stream_read32(&table);
      if (stream_read_bits(v, 32, 16, 3) != 0x07)
        {
          stream_destruct(&table);
          ts_psi_destruct(&psi);
          return -1;
        }
      pat->id = psi.id;
      pat->id_extension = psi.id_extension;
      pat->version = psi.version;
      pat->program_number = stream_read_bits(v, 32, 0, 16);
      pat->program_pid = stream_read_bits(v, 32, 19, 13);
      fprintf(stderr, "%u %u\n", pat->program_number, pat->program_pid);
    }

  stream_read(data, NULL, psi.section_length - 9);
  // checksum
  (void) stream_read32(data);

  fprintf(stderr, "%lu\n", stream_size(data));
  fprintf(stderr, "%02x\n", stream_read8(data));

  stream_destruct(&table);
  ts_psi_destruct(&psi);

  return stream_valid(data) ? 1 : -1;
}

ssize_t ts_pat_unpack_buffer(ts_pat *pat, buffer *buffer)
{
  stream stream;
  ssize_t n;

  stream_construct_buffer(&stream, buffer);
  n = ts_pat_unpack_stream(pat, &stream);
  stream_destruct(&stream);

  return n;
}

