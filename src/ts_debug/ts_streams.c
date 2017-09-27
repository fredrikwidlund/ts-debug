#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <err.h>

#include <dynamic.h>

#include "ts_packet.h"
#include "ts_packets.h"
#include "ts_psi.h"
#include "ts_pes.h"
#include "ts_stream.h"
#include "ts_streams.h"

/* internals */

static void ts_streams_streams_release(void *object)
{
  ts_stream_destruct(*(ts_stream **) object);
}

static int ts_streams_complete(ts_streams *streams)
{
  ts_stream **i;

  list_foreach(&streams->streams, i)
    (void) ts_stream_write(*i, NULL);

  return 0;
}

/* ts_streams */


void ts_streams_construct(ts_streams *streams)
{
  ts_psi_construct(&streams->psi);
  list_construct(&streams->streams);
}

void ts_streams_destruct(ts_streams *streams)
{
  list_destruct(&streams->streams, ts_streams_streams_release);
}

ts_stream *ts_streams_create(ts_streams *streams, int pid, int type, int content_type)
{
  ts_stream *stream, **i;

  stream = malloc(sizeof *stream);
  if (!stream)
    abort();
  if (pid == 0 && type == TS_STREAM_TYPE_UNKNOWN)
    type = TS_STREAM_TYPE_PSI;
  ts_stream_construct(stream, streams, pid, type, content_type);
  list_foreach(&streams->streams, i)
    if (stream->pid < (*i)->pid)
      break;
  list_insert(i, &stream, sizeof stream);
  return stream;
}

ts_stream *ts_streams_lookup(ts_streams *streams, int pid)
{
  ts_stream **i;

  list_foreach(&streams->streams, i)
    if ((*i)->pid == pid)
      return *i;
  return NULL;
}

int ts_streams_write(ts_streams *streams, ts_packets *packets)
{
  ts_packet *packet;
  ts_stream *stream;
  int e;

  if (!packets)
    return ts_streams_complete(streams);

  while (1)
    {
      packet = ts_packets_read(packets);
      if (!packet)
        return 0;

      if (packet->pid == 0x1fff)
        continue;

      stream = ts_streams_lookup(streams, packet->pid);
      if (!stream)
        stream = ts_streams_create(streams, packet->pid, TS_STREAM_TYPE_UNKNOWN, 0);

      e = ts_stream_write(stream, packet);
      if (e == -1)
        return -1;
    }

  return 0;
}

int ts_streams_pack(ts_streams *streams, ts_packets *packets)
{
  ts_stream *stream;

  stream = ts_streams_lookup(streams, 0);

  return ts_stream_pack(stream, packets);
}

void ts_streams_debug(ts_streams *streams, FILE *f, int indent)
{
  ts_stream **i;
  int n;

  ts_psi_debug(&streams->psi, f, indent);

  n = 0;
  list_foreach(&streams->streams, i)
    {
      (void) fprintf(f, "%*s[stream %d]\n", indent * 2, "", n);
      ts_stream_debug(*i, f, indent + 1);
      n ++;
    }
}
