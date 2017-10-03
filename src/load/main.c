#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <err.h>

#include <dynamic.h>

#include "ts_adaptation_field.h"
#include "ts_packet.h"
#include "ts_packets.h"
#include "ts_stream.h"
#include "ts.h"

int load(buffer *buffer, char *path)
{
  int fd;
  char block[1048576];
  ssize_t n;

  fd = open(path, O_RDONLY);
  if (fd == -1)
    return -1;

  while (1)
    {
      n = read(fd, block, sizeof block);
      if (n <= 0)
        break;
      buffer_insert(buffer, buffer_size(buffer), block, n);
    }
  (void) close(fd);
  buffer_compact(buffer);

  return n;
}

int save(buffer *buffer, char *path)
{
  int fd;
  char *data;
  size_t size;
  ssize_t n;

  fd = open(path, O_CREAT | O_WRONLY, 0644);
  if (fd == -1)
    return -1;

  data = buffer_data(buffer);
  size = buffer_size(buffer);
  while (size)
    {
      n = write(fd, data, size);
      if (n == -1)
        {
          (void) close(fd);
          return -1;
        }
      data += n;
      size -= n;
    }

  return 0;
}

static void resync(ts *ts)
{
  (void) ts;
}

int main(int argc, char **argv)
{
  buffer buffer;
  stream stream;
  ts ts;
  int e;

  if (argc != 3)
    errx(1, "invalid parameters\n");

  buffer_construct(&buffer);
  e = load(&buffer, argv[1]);
  if (e == -1)
    err(1, "load %s\n", argv[1]);
  stream_construct_buffer(&stream, &buffer);

  ts_construct(&ts);
  ts_unpack(&ts, &stream);
  stream_destruct(&stream);
  buffer_destruct(&buffer);

  ts_debug(&ts);
  resync(&ts);

  buffer_construct(&buffer);
  stream_construct_buffer(&stream, &buffer);
  ts_pack(&ts, &stream);
  ts_destruct(&ts);
  stream_destruct(&stream);

  e = save(&buffer, argv[2]);
  if (e == -1)
    err(1, "save %s\n", argv[2]);
  buffer_destruct(&buffer);
}
