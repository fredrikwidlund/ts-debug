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

static ssize_t ts_psi_parse_pat(ts_psi *psi, void *data, size_t size, int id_extension, int version)
{
  bytes b;
  uint16_t num, v, pid;

  if (size < 4)
    return 0;

  if (size > 4)
    return -1;

  bytes_construct(&b, data, 4);
  num = bytes_read2(&b);
  v = bytes_read2(&b);
  if (bytes_bits(v, 16, 0, 3) != 0x07)
    return 0;
  pid = bytes_bits(v, 16, 3, 13);

  psi->pat.present = 1;
  psi->pat.id_extension = id_extension;
  psi->pat.version = version;
  psi->pat.program_number = num;
  psi->pat.program_pid = pid;

  return 1;
}

static ssize_t ts_psi_parse_pmt(ts_psi *psi, void *data, size_t size, int id_extension, int version)
{
  bytes b;
  uint32_t v;
  int type;
  size_t len;

  if (size < 4)
    return 0;

  bytes_construct(&b, data, size);
  v = bytes_read4(&b);
  if (bytes_bits(v, 32, 0, 3) != 0x07 ||
      bytes_bits(v, 32, 16, 4) != 0x0f ||
      bytes_bits(v, 32, 20, 2) != 0x00)
    return -1;
  len = bytes_bits(v, 32, 22, 10);
  bytes_read(&b, NULL, len);

  psi->pmt.present = 1;
  psi->pmt.id_extension = id_extension;
  psi->pmt.version = version;
  psi->pmt.pcr_pid = bytes_bits(v, 32, 3, 13);

  while (bytes_size(&b))
    {
      type = bytes_read1(&b);
      printf("type %d\n", type);
      v = bytes_read4(&b);
      printf("%lu %lu %lu %lu %lu\n",
             bytes_bits(v, 32, 0, 3),
             bytes_bits(v, 32, 3, 13),
             bytes_bits(v, 32, 16, 4),
             bytes_bits(v, 32, 20, 2),
             bytes_bits(v, 32, 22, 10));
    }

  return 1;
}

ssize_t ts_psi_parse(ts_psi *psi, void *data, size_t size)
{
  bytes b, syntax;
  uint32_t v, id_extension;
  int id, len, version, current, section, last_section;
  ssize_t n;

  *psi = (ts_psi) {0};

  bytes_construct(&b, data, size);
  v = bytes_read1(&b);
  bytes_read(&b, NULL, v);

  while (bytes_size(&b))
    {
      id = bytes_read1(&b);
      if (id == 0xff)
        return 1;

      v = bytes_read2(&b);
      if (!bytes_valid(&b))
        return 0;

      if (bytes_bits(v, 16, 0, 1) != 0x01 ||
          bytes_bits(v, 16, 1, 1) != 0x00 ||
          bytes_bits(v, 16, 2, 2) != 0x03 ||
          bytes_bits(v, 16, 4, 2) != 0x00)
        return -1;

      len = bytes_bits(v, 16, 6, 10);
      syntax = b;
      if (bytes_valid(&syntax) && len < 4)
        return -1;
      bytes_truncate(&syntax, len - 4);
      id_extension = bytes_read2(&syntax);
      v = bytes_read1(&syntax);
      if (bytes_bits(v, 8, 0, 2) != 0x03)
        return -1;

      version = bytes_bits(v, 8, 2, 5);
      current = bytes_bits(v, 8, 7, 1);
      section = bytes_read1(&syntax);
      last_section = bytes_read1(&syntax);

      if (!bytes_valid(&syntax))
        return 0;

      if (current)
        switch (id)
          {
          case 0x00:
            n = ts_psi_parse_pat(psi, bytes_data(&syntax), bytes_size(&syntax), id_extension, version);
            if (n <= 0)
              return n;
            break;
          case 0x02:
            n = ts_psi_parse_pmt(psi, bytes_data(&syntax), bytes_size(&syntax), id_extension, version);
            if (n <= 0)
              return n;
            break;
          default:
            break;
          }

      if (section == last_section)
        return 1;

      bytes_read(&b, NULL, len);
    }

  return 0;
}
