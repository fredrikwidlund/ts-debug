#ifndef TS_STREAMS_H_INCLUDED
#define TS_STREAMS_H_INCLUDED

struct ts_streams
{
  ts_psi      psi;
  list        streams;
};

void       ts_streams_construct(ts_streams *);
void       ts_streams_destruct(ts_streams *);
ts_stream *ts_streams_create(ts_streams *, int, int, int);
ts_stream *ts_streams_lookup(ts_streams *, int);
int        ts_streams_write(ts_streams *, ts_packets *);
int        ts_streams_pack(ts_streams *, ts_packets *);
void       ts_streams_debug(ts_streams *, FILE *, int);

#endif /* TS_STREAM_H_INCLUDED */
