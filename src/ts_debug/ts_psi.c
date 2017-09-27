#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <err.h>

#include <dynamic.h>

#include "ts_psi.h"

/* internals */

static void ts_psi_pmt_debug(ts_psi_pmt *pmt, FILE *f, int indent)
{
  ts_psi_pmt_stream *stream;

  if (!pmt->present)
    return;
  (void) fprintf(f, "%*s[pmt id extension %d, version %d, pcr pid %d]\n", indent * 2, "",
                 pmt->id_extension, pmt->version, pmt->pcr_pid);
  indent ++;
  list_foreach(&pmt->streams, stream)
    (void) fprintf(f, "%*s[stream %d, type %d]\n", indent * 2, "", stream->pid, stream->type);
}

static void ts_psi_pat_debug(ts_psi_pat *pat, FILE *f, int indent)
{
  if (!pat->present)
    return;

  (void) fprintf(f, "%*s[pat id extension %d, version %d, program number %d, pmt pid %d]\n", indent * 2, "",
                 pat->id_extension, pat->version, pat->program_number, pat->program_pid);
}

static ssize_t ts_psi_pmt_unpack(ts_psi_pmt *pmt, void *data, size_t size, int id_extension, int version)
{
  stream s;
  uint32_t v;
  int type, pid, valid;
  size_t len;

  stream_construct(&s, data, size);
  v = stream_read32(&s);
  if (stream_read_bits(v, 32, 0, 3) != 0x07 ||
      stream_read_bits(v, 32, 16, 4) != 0x0f ||
      stream_read_bits(v, 32, 20, 2) != 0x00)
    {
      stream_destruct(&s);
      return -1;
    }
  pid = stream_read_bits(v, 32, 3, 13);
  len = stream_read_bits(v, 32, 22, 10);
  stream_read(&s, NULL, len);

  pmt->present = 1;
  pmt->id_extension = id_extension;
  pmt->version = version;
  pmt->pcr_pid = pid;

  while (stream_size(&s))
    {
      type = stream_read8(&s);
      v = stream_read32(&s);
      if (stream_read_bits(v, 32, 0, 3) != 0x07 ||
          stream_read_bits(v, 32, 16, 4) != 0x0f ||
          stream_read_bits(v, 32, 20, 2) != 0x00)
        {
          stream_destruct(&s);
          return -1;
        }
      pid = stream_read_bits(v, 32, 3, 13);
      len = stream_read_bits(v, 32, 22, 10);
      stream_read(&s, NULL, len);
      list_push_back(&pmt->streams, (ts_psi_pmt_stream[]){{.type = type, .pid = pid}}, sizeof (ts_psi_pmt_stream));
    }

  valid = stream_valid(&s);
  stream_destruct(&s);
  return valid ? (ssize_t) size : -1;
}

/* internals */

static ssize_t ts_psi_pat_unpack(ts_psi_pat *pat, void *data, size_t size, int id_extension, int version)
{
  stream s;
  uint32_t v;
  int num, pid, valid;

  stream_construct(&s, data, size);
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

  valid = stream_valid(&s);
  stream_destruct(&s);
  return valid ? (ssize_t) size : -1;
}

/*
static ssize_t ts_psi_pack_pat(ts_psi *psi, bytes *b)
{
  uint32_t v;

  v = bytes_write_bits(psi->pat.program_number, 32, 0, 16);
  v |= bytes_write_bits(0x07, 32, 16, 3);
  v |= bytes_write_bits(psi->pat.program_pid, 32, 19, 13);
  bytes_write32(b, v);

  return 4;
}
*/

static ssize_t ts_psi_unpack_table_data(ts_psi *psi, void *data, size_t size, int id, int id_extension, int version)
{
  switch (id)
    {
    case 0x00:
      return ts_psi_pat_unpack(&psi->pat, data, size, id_extension, version);
    case 0x02:
      return ts_psi_pmt_unpack(&psi->pmt, data, size, id_extension, version);
    default:
      return size;
    }
}

/* ts_psi */

void ts_psi_construct(ts_psi *psi)
{
  *psi = (ts_psi) {0};
  list_construct(&psi->pmt.streams);
}

void ts_psi_destruct(ts_psi *psi)
{
  list_destruct(&psi->pmt.streams, NULL);
}

ssize_t ts_psi_unpack(ts_psi *psi, void *data, size_t size)
{
  stream s;
  uint32_t v;
  size_t len;
  int id, id_extension, version, current, section, section_last;
  ssize_t n;

  stream_construct(&s, data, size);

  // table pointer
  len = stream_read8(&s);
  stream_read(&s, NULL, len);

  // read potentially multiple headers
  while (stream_size(&s))
    {
      // table id
      id = stream_read8(&s);
      if (id == 0xff)
        return size;

      // table header section
      v = stream_read16(&s);
      if (!stream_valid(&s))
        return 0;
      if (stream_read_bits(v, 16, 0, 1) != 0x01 ||
          stream_read_bits(v, 16, 1, 1) != 0x00 ||
          stream_read_bits(v, 16, 2, 2) != 0x03 ||
          stream_read_bits(v, 16, 4, 2) != 0x00)
        return -1;

      // calculate table data length
      len = stream_read_bits(v, 16, 6, 10);
      if (len < 9 || stream_size(&s) < len)
        return -1;
      len -= 9;

      // table syntax section
      id_extension = stream_read16(&s);
      v = stream_read8(&s);
      if (stream_read_bits(v, 8, 0, 2) != 0x03)
        return -1;
      version = stream_read_bits(v, 8, 2, 5);
      current = stream_read_bits(v, 8, 7, 1);
      section = stream_read8(&s);
      section_last = stream_read8(&s);

      // unpack table data if current
      if (current)
        {
          n = ts_psi_unpack_table_data(psi, stream_data(&s), len, id, id_extension, version);
          if (n == -1)
            return -1;
        }
      stream_read(&s, NULL, len);
      stream_read(&s, NULL, 4);
      if (!stream_valid(&s))
        return -1;

      if (section == section_last)
        return size;
    }

  return 0;
}

void ts_psi_debug(ts_psi *psi, FILE *f, int indent)
{
  ts_psi_pat_debug(&psi->pat, f, indent);
  ts_psi_pmt_debug(&psi->pmt, f, indent);
}

/*
ssize_t ts_psi_pack(ts_psi *psi, void **data, size_t *size, int content_type)
{
  buffer buffer, content;
  uint64_t v;
  bytes b, c;

  switch (content_type)
    {
    case 0x00:
      buffer_construct(&content);
      bytes_construct_buffer(&c, &content);
      ts_psi_pack_pat(psi, &c);
      break;
    }

  buffer_construct(&buffer);
  bytes_construct_buffer(&b, &buffer);
  bytes_write8(&b, 0);
  bytes_write8(&b, 0);

  v =
    bytes_write_bits(1, 16, 0, 1) |
    bytes_write_bits(1, 16, 1, 1) |
    bytes_write_bits(3, 16, 2, 2) |
    bytes_write_bits(0, 16, 4, 2) |
    bytes_write_bits(bytes_size(&c), 16, 6, 10);
  bytes_write16(&b, v);

  buffer_debug(stderr, b.buffer);
  buffer_debug(stderr, c.buffer);

  return -1;
}
*/
