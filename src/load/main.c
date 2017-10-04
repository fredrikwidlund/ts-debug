#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <err.h>

#include <dynamic.h>

#include "ts_adaptation_field.h"
#include "ts_pes.h"
#include "ts_packet.h"
#include "ts_packets.h"
#include "ts_stream.h"
#include "ts.h"

ssize_t buffer_construct_file(buffer *buffer, char *path)
{
  int fd;
  char block[1048576];
  ssize_t n;

  fd = open(path, O_RDONLY);
  if (fd == -1)
    return -1;

  buffer_construct(buffer);
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

ssize_t buffer_save(buffer *buffer, char *path)
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
  ts ts;
  buffer buffer;
  ssize_t n;

  if (argc != 3)
    errx(1, "invalid parameters\n");

  n = buffer_construct_file(&buffer, argv[1]);
  if (n == -1)
    err(1, "buffer_construct_file %s\n", argv[1]);

  ts_construct(&ts);
  n = ts_unpack_buffer(&ts, &buffer);
  buffer_destruct(&buffer);
  if (n == -1)
    errx(1, "ts_construct_buffer");

  ts_close(&ts);
  ts_debug(&ts);
  resync(&ts);

  buffer_construct(&buffer);
  ts_pack_buffer(&ts, &buffer);
  ts_destruct(&ts);

  n = buffer_save(&buffer, argv[2]);
  buffer_destruct(&buffer);
  if (n == -1)
    err(1, "save %s\n", argv[2]);
}
