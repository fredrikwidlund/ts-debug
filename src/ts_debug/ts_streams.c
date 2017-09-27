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

static void ts_streams_list_release(void *object)
{
  ts_stream *stream = *(ts_stream **) object;

  ts_stream_destruct(stream);
  free(stream);
}

static int ts_streams_complete(ts_streams *streams)
{
  ts_stream **i;

  list_foreach(&streams->list, i)
    (void) ts_stream_unpack(*i, NULL);

  return 0;
}

static ts_stream *ts_streams_create(ts_streams *streams, int pid, int type, int content_type)
{
  ts_stream *stream, **i;

  stream = malloc(sizeof *stream);
  if (!stream)
    abort();
  if (pid == 0 && type == TS_STREAM_TYPE_UNKNOWN)
    type = TS_STREAM_TYPE_PSI;
  ts_stream_construct(stream, streams, pid, type, content_type);
  list_foreach(&streams->list, i)
    if (stream->pid < (*i)->pid)
      break;
  list_insert(i, &stream, sizeof stream);
  return stream;
}

static int ts_streams_type(ts_streams *streams, int pid, int type, int content_type)
{
  ts_stream *stream;

  stream = ts_streams_lookup(streams, pid);
  if (stream)
    return ts_stream_type(stream, type, content_type);
  else
    {
      (void) ts_streams_create(streams, pid, type, content_type);
      return 0;
    }
}

/* ts_streams */

void ts_streams_construct(ts_streams *streams)
{
  ts_psi_construct(&streams->psi);
  list_construct(&streams->list);
}

void ts_streams_destruct(ts_streams *streams)
{
  ts_psi_destruct(&streams->psi);
  list_destruct(&streams->list, ts_streams_list_release);
}

ts_stream *ts_streams_lookup(ts_streams *streams, int pid)
{
  ts_stream **i;

  list_foreach(&streams->list, i)
    if ((*i)->pid == pid)
      return *i;
  return NULL;
}

ssize_t ts_streams_unpack(ts_streams *streams, ts_packets *packets)
{
  ts_packet **i, *packet;
  ts_stream *stream;
  ssize_t e, n;

  if (!packets)
    return ts_streams_complete(streams);

  n = 0;
  list_foreach(ts_packets_list(packets), i)
    {
      packet = *i;
      if (packet->pid == 0x1fff)
        continue;

      stream = ts_streams_lookup(streams, packet->pid);
      if (!stream)
        stream = ts_streams_create(streams, packet->pid, TS_STREAM_TYPE_UNKNOWN, 0);

      e = ts_stream_unpack(stream, packet);
      if (e == -1)
        return -1;
      n += e;
    }

  return n;
}

ssize_t ts_streams_pack(ts_streams *streams, ts_packets *packets)
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
  list_foreach(&streams->list, i)
    {
      (void) fprintf(f, "%*s[stream %d]\n", indent * 2, "", n);
      ts_stream_debug(*i, f, indent + 1);
      n ++;
    }
}

int ts_streams_psi(ts_streams *streams, ts_psi *psi)
{
  ts_psi_pmt_stream *i;
  int e;

  if (psi->pat.present && !streams->psi.pat.present)
    {
      streams->psi.pat = psi->pat;
      e = ts_streams_type(streams, streams->psi.pat.program_pid, TS_STREAM_TYPE_PSI, 0);
      if (e == -1)
        return -1;
    }

  if (psi->pmt.present && !streams->psi.pmt.present)
    {
      streams->psi.pmt.present = 1;
      streams->psi.pmt.id_extension = psi->pmt.id_extension;
      streams->psi.pmt.version = psi->pmt.version;
      streams->psi.pmt.pcr_pid = psi->pmt.pcr_pid;
      list_foreach(&psi->pmt.streams, i)
        {
          e = ts_streams_type(streams, i->pid, TS_STREAM_TYPE_PES, i->type);
          if (e == -1)
            return -1;
          list_push_back(&streams->psi.pmt.streams, i, sizeof i);
        }
    }

  return 0;
}
