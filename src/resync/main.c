#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>
#include <dynamic.h>

typedef struct ts_packet ts_packet;
typedef struct ts_adaptation_field ts_adaptation_field;

struct ts_adaptation_field
{
  unsigned             discontinuity_indicator:1;
  unsigned             random_access_indicator:1;
  unsigned             elementary_stream_priority_indicator:1;
  unsigned             pcr_flag:1;
  unsigned             opcr_flag:1;
  unsigned             splicing_point_flag:1;
  unsigned             transport_private_data_flag:1;
  unsigned             adaptation_field_extension_flag:1;
  uint64_t             pcr;
  uint64_t             opcr;
  uint8_t              splicing_countdown;
};

typedef struct ts_packet ts_packet;
struct ts_packet
{
  unsigned             transport_error_indicator:1;
  unsigned             payload_unit_start_indicator:1;
  unsigned             transport_priority:1;
  unsigned             pid:13;
  unsigned             transport_scrambling_control:2;
  unsigned             adaptation_field_control:2;
  unsigned             continuity_counter:4;
  ts_adaptation_field  adaptation_field;
  void                *data;
  size_t               size;
};

void ts_packet_construct(ts_packet *packet)
{
  *packet = (ts_packet) {0};
}

void ts_packet_destruct(ts_packet *packet)
{
  free(packet->data);
  free(packet);
}

ssize_t ts_packet_unpack(ts_packet *packet, stream *stream)
{
  if (stream_size(stream) < 188)
    return 0;

  packet->size = 188;
  packet->data = malloc(packet->size);
  if (!packet->data)
    abort();
  stream_read(stream, packet->data, packet->size);
  if (!stream_valid(stream))
    {
      ts_packet_destruct(packet);
      return -1;
    }

  return 1;
}

ssize_t ts_packet_pack(ts_packet *packet, stream *stream)
{
  stream_write(stream, packet->data, packet->size);
  return stream_valid(stream) ? 1 : -1;
}

ssize_t ts_packets_unpack(list *packets, stream *stream)
{
  ts_packet *packet;
  ssize_t n, count;

  count = 0;
  while (stream_size(stream))
    {
      packet = malloc(sizeof *packet);
      if (!packet)
        abort();
      ts_packet_construct(packet);
      n = ts_packet_unpack(packet, stream);
      if (n <= 0)
        {
          ts_packet_destruct(packet);
          return n == -1 ? -1 : count;
        }
      list_push_back(packets, &packet, sizeof packet);
      count += n;
    }

  return count;
}

ssize_t ts_packets_pack(list *packets, stream *stream)
{
  ts_packet **i;
  ssize_t n, count;

  count = 0;
  list_foreach(packets, i)
    {
      n = ts_packet_pack(*i, stream);
      if (n == -1)
        return -1;
      if (n == 0)
        break;
      count += n;
    }
  return count;
}

void packets_release(void *object)
{
  ts_packet_destruct(*(ts_packet **) object);
}

int main()
{
  buffer buffer;
  stream stream;
  list packets;
  char data[1048476], *p;
  ssize_t n, size;
  ts_packet **i;

  buffer_construct(&buffer);
  while (1)
    {
      n = read(0, data, sizeof data);
      if (n == -1)
        err(1, "read");
      if (n == 0)
        break;
      buffer_insert(&buffer, buffer_size(&buffer), data, n);
    }
  (void) fprintf(stderr, "read %ld bytes\n", buffer_size(&buffer));

  list_construct(&packets);
  stream_construct_buffer(&stream, &buffer);
  n = ts_packets_unpack(&packets, &stream);
  if (n == -1)
    err(1, "ts_packets_unpack");
  (void) fprintf(stderr, "read %ld packets\n", n);
  stream_destruct(&stream);
  buffer_destruct(&buffer);

  list_foreach(&packets, i)
    (*i)->adaptation_field.pcr_flag = 0;

  buffer_construct(&buffer);
  stream_construct_buffer(&stream, &buffer);
  n = ts_packets_pack(&packets, &stream);
  if (n == -1)
    err(1, "ts_packets_pack");
  list_destruct(&packets, packets_release);
  stream_destruct(&stream);
  (void) fprintf(stderr, "wrote %ld packets\n", n);

  p = buffer_data(&buffer);
  size = buffer_size(&buffer);
  while (size)
    {
      n = write(1, p, size);
      if (n == -1)
        err(1, "write");
      p += n;
      size -= n;
    }
  buffer_destruct(&buffer);

  (void) fprintf(stderr, "wrote %ld bytes\n", n);
}
