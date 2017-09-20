#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <err.h>

#include <dynamic.h>

#include "bytes.h"
#include "ts_psi.h"

/* internals */

static void buffer_debug(FILE *f, buffer *b)
{
  size_t i, size;
  uint8_t *data;

  data = buffer_data(b);
  size = buffer_size(b);
  (void) fprintf(f, "[%lu]", size);
  for (i = 0; i < size; i ++)
    {
      (void) fprintf(f, "%s", i % 16 ? " " : "\n");
      (void) fprintf(f, "0x%02x", data[i]);
    }
  (void) fprintf(f, "\n");
}

static ssize_t ts_psi_unpack_pmt(ts_psi *psi, void *data, size_t size, int id_extension, int version)
{
  bytes b;
  uint32_t v;
  int type, pid;
  size_t len;

  bytes_construct(&b, data, size);
  v = bytes_read4(&b);
  if (bytes_bits(v, 32, 0, 3) != 0x07 ||
      bytes_bits(v, 32, 16, 4) != 0x0f ||
      bytes_bits(v, 32, 20, 2) != 0x00)
    return -1;
  pid = bytes_bits(v, 32, 3, 13);
  len = bytes_bits(v, 32, 22, 10);
  bytes_read(&b, NULL, len);

  psi->pmt.present = 1;
  psi->pmt.id_extension = id_extension;
  psi->pmt.version = version;
  psi->pmt.pcr_pid = pid;

  while (bytes_size(&b))
    {
      type = bytes_read1(&b);
      v = bytes_read4(&b);
      if (bytes_bits(v, 32, 0, 3) != 0x07 ||
          bytes_bits(v, 32, 16, 4) != 0x0f ||
          bytes_bits(v, 32, 20, 2) != 0x00)
        return -1;
      pid = bytes_bits(v, 32, 3, 13);
      len = bytes_bits(v, 32, 22, 10);
      bytes_read(&b, NULL, len);
      list_push_back(&psi->pmt.streams, (ts_psi_pmt_stream[]){{.type = type, .pid = pid}}, sizeof (ts_psi_pmt_stream));
    }

 return bytes_valid(&b) ? (ssize_t) size : -1;
}

/* internals */

static ssize_t ts_psi_unpack_pat(ts_psi *psi, void *data, size_t size, int id_extension, int version)
{
  bytes b;
  uint32_t v;
  int num, pid;

  bytes_construct(&b, data, size);
  while (bytes_size(&b))
    {
      if (psi->pat.present)
        return -1;
      v = bytes_read4(&b);
      if (bytes_bits(v, 32, 16, 3) != 0x07)
        return -1;
      num = bytes_bits(v, 32, 0, 16);
      pid = bytes_bits(v, 32, 19, 13);

      psi->pat.present = 1;
      psi->pat.id_extension = id_extension;
      psi->pat.version = version;
      psi->pat.program_number = num;
      psi->pat.program_pid = pid;
    }

  return bytes_valid(&b) ? (ssize_t) size : -1;
}

static ssize_t ts_psi_pack_pat(ts_psi *psi, bytes *b)
{
  uint32_t v;

  v = bytes_write_bits(psi->pat.program_number, 32, 0, 16);
  v |= bytes_write_bits(0x07, 32, 16, 3);
  v |= bytes_write_bits(psi->pat.program_pid, 32, 19, 13);
  bytes_write32(b, v);

  return 4;
}

static ssize_t ts_psi_unpack_table_data(ts_psi *psi, void *data, size_t size, int id, int id_extension, int version)
{
  switch (id)
    {
    case 0x00:
      return ts_psi_unpack_pat(psi, data, size, id_extension, version);
    case 0x02:
      return ts_psi_unpack_pmt(psi, data, size, id_extension, version);
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
  bytes b;
  uint32_t v;
  size_t len;
  int id, id_extension, version, current, section, section_last;
  ssize_t n;

  // table pointer
  bytes_construct(&b, data, size);
  v = bytes_read1(&b);
  bytes_read(&b, NULL, v);

  // read potentially multiple headers
  while (bytes_size(&b))
    {
      // table id
      id = bytes_read1(&b);
      if (id == 0xff)
        return size;

      // table header section
      v = bytes_read2(&b);
      if (!bytes_valid(&b))
        return 0;
      if (bytes_bits(v, 16, 0, 1) != 0x01 ||
          bytes_bits(v, 16, 1, 1) != 0x00 ||
          bytes_bits(v, 16, 2, 2) != 0x03 ||
          bytes_bits(v, 16, 4, 2) != 0x00)
        return -1;

      // calculate table data length
      len = bytes_bits(v, 16, 6, 10);
      if (len < 9 || bytes_size(&b) < len)
        return -1;
      len -= 9;

      // table syntax section
      id_extension = bytes_read2(&b);
      v = bytes_read1(&b);
      if (bytes_bits(v, 8, 0, 2) != 0x03)
        return -1;
      version = bytes_bits(v, 8, 2, 5);
      current = bytes_bits(v, 8, 7, 1);
      section = bytes_read1(&b);
      section_last = bytes_read1(&b);

      // unpack table data if current
      if (current)
        {
          n = ts_psi_unpack_table_data(psi, bytes_data(&b), len, id, id_extension, version);
          if (n == -1)
            return -1;
        }
      bytes_read(&b, NULL, len);
      bytes_read(&b, NULL, 4);
      if (!bytes_valid(&b))
        return -1;

      if (section == section_last)
        return size;
    }

  return 0;
}

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
  /*
  switch (content_type)
    {
    case 0x00:
      return ts_psi_pack_pat(psi, data, size);
    default:
      return -1;
    }
  */
}
