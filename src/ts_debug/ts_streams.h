#ifndef TS_STREAMS_H_INCLUDED
#define TS_STREAMS_H_INCLUDED

struct ts_streams
{
  ts_psi      psi;
  list        list;
};

void       ts_streams_construct(ts_streams *);
void       ts_streams_destruct(ts_streams *);
ts_stream *ts_streams_lookup(ts_streams *, int);
ssize_t    ts_streams_unpack(ts_streams *, ts_packets *);
ssize_t    ts_streams_pack(ts_streams *, ts_packets *);

int        ts_streams_psi(ts_streams *, ts_psi *);
void       ts_streams_debug(ts_streams *, FILE *, int);

#endif /* TS_STREAM_H_INCLUDED */
