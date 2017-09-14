#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "bytes.h"

static void bytes_invalid(bytes *b)
{
  b->base = NULL;
  b->end = NULL;
}

static int bytes_need(bytes *b, size_t size)
{
  if (!bytes_valid(b) || size > bytes_size(b))
    {
      bytes_invalid(b);
      return 0;
    }

  return 1;
}

void bytes_construct(bytes *b, void *data, size_t size)
{
  b->base = data;
  b->end = b->base + size;
}

int bytes_valid(bytes *b)
{
  return b->base != NULL;
}

void bytes_truncate(bytes *b, size_t size)
{
  if (!bytes_need(b, size))
    return;
  b->end = b->base + size;
}

size_t bytes_size(bytes *b)
{
  return b->end - b->base;
}

void bytes_read(bytes *b, void **data, size_t size)
{
  if (!bytes_need(b, size))
    {
      if (data)
        *data = NULL;
      return;
    }

  if (data)
    *data = b->base;
  b->base += size;
}

uint8_t bytes_read1(bytes *b)
{
  uint8_t v;

  if (!bytes_need(b, 1))
    return 0;

  v = b->base[0];
  b->base ++;
  return v;
}

uint16_t bytes_read2(bytes *b)
{
  uint16_t v;

  if (!bytes_need(b, 2))
    return 0;

  v = ((uint16_t) b->base[0] << 8) + b->base[1];
  b->base += 2;
  return v;
}

uint32_t bytes_read4(bytes *b)
{
  return ((uint32_t) bytes_read2(b) << 16) + bytes_read2(b);
}

uint64_t bytes_read6(bytes *b)
{
  return ((uint64_t) bytes_read2(b) << 32) + bytes_read4(b);
}

uint64_t bytes_read8(bytes *b)
{
  return ((uint64_t) bytes_read4(b) << 32) + bytes_read4(b);
}

uint64_t bytes_bits(uint64_t value, int size, int offset, int bits)
{
  value >>= (size - offset - bits);
  value &= (1 << bits) - 1;
  return value;
}

uint8_t bytes_index(bytes *b, size_t index)
{
  if (!bytes_need(b, index))
    return 0;

  return (uint8_t) b->base[index];
}

void bytes_rest(bytes *b, void **data, size_t *size)
{
  *data = b->base;
  *size = bytes_size(b);
  b->base = b->end;
}

void *bytes_data(bytes *b)
{
  return b->base;
}
