#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>

#include <dynamic.h>

#include "bytes.h"

typedef struct ts_packet ts_packet;
typedef struct ts ts;

typedef struct ts_adaptation_field ts_adaptation_field;
struct ts_adaptation_field
{
  int x;
};

struct ts_packet
{
  void                *data;
  size_t               size;

  unsigned             transport_error_indicator:1;
  unsigned             payload_unit_start_indicator:1;
  unsigned             transport_priority:1;
  unsigned             pid:13;
  unsigned             transport_scrambling_control:2;
  unsigned             adaptation_field_control:2;
  unsigned             continuity_counter:4;
  ts_adaptation_field  adaptation_field;
  void                *payload_data;
  size_t               payload_size;
};

struct ts
{
  list      packets;
};

void ts_construct(ts *ts)
{
  list_construct(&ts->packets);
}

ssize_t ts_adaptation_field_parse(ts_adaptation_field *af, void *data, size_t size)
{
  bytes b;
  int len;

  bytes_construct(&b, data, size);
  len = bytes_read1(&b);

  return len + 1;
}

ssize_t ts_packet_parse(ts_packet *p, void *data, size_t size)
{
  bytes b;
  uint32_t v;
  ssize_t n;

  if (size != 188)
    return -1;

  p->size = size;
  p->data = malloc(p->size);
  if (!p->data)
    abort();
  memcpy(p->data, data, p->size);

  bytes_construct(&b, data, size);
  v = bytes_read4(&b);
  p->transport_error_indicator = bytes_bits(v, 32, 8, 1);
  p->payload_unit_start_indicator = bytes_bits(v, 32, 9, 1);
  p->transport_priority = bytes_bits(v, 32, 10, 1);
  p->pid = bytes_bits(v, 32, 11, 13);
  p->transport_scrambling_control = bytes_bits(v, 32, 24, 2);
  p->adaptation_field_control = bytes_bits(v, 32, 26, 2);
  p->continuity_counter = bytes_bits(v, 32, 28, 4);

  if (p->adaptation_field_control & 0x02)
    {
      n = ts_adaptation_field_parse(&p->adaptation_field, bytes_data(&b), bytes_size(&b));
      if (n == -1)
        return -1;
      bytes_read(&b, NULL, n);
    }

  if (p->adaptation_field_control & 0x01)
    {
      printf("payload %lu\n", bytes_size(&b));
    }

  return 0;
}

void ts_packet_delete(ts_packet *p)
{
  free(p->data);
  free(p);
}

ts_packet *ts_packet_new(void *data, size_t size)
{
  ts_packet *p;
  int e;

  p = calloc(1, sizeof *p);
  if (!p)
    abort();

  e = ts_packet_parse(p, data, size);
  if (e == -1)
    {
      ts_packet_delete(p);
      return NULL;
    }

  return p;
}

int ts_write(ts *ts, void *data, size_t size)
{
  ts_packet *p;

  p = ts_packet_new(data, size);
  if (!p)
    return -1;

  list_push_back(&ts->packets, &p, sizeof p);
  return 0;
}

int main()
{
  ts ts;
  char packet[188];
  ssize_t n;

  ts_construct(&ts);

  while (1)
    {
      n = fread(packet, sizeof packet, 1, stdin);
      if (n != 1)
        break;

      ts_write(&ts, packet, sizeof packet);
    }
}
