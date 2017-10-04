#ifndef TS_H_INCLUDED
#define TS_H_INCLUDED

enum ts_status
{
  TS_STATUS_CLOSED,
  TS_STATUS_OPEN
};

typedef struct ts ts;
struct ts
{
  int  status;
  list streams;
};

void       ts_construct(ts *);
void       ts_destruct(ts *);
void       ts_close(ts *);
ts_stream *ts_lookup(ts *, int);
ts_stream *ts_create(ts *, int);
ssize_t    ts_pack_packets(ts *, ts_packets *);
ssize_t    ts_pack_stream(ts *, stream *);
ssize_t    ts_pack_buffer(ts *, buffer *);
ssize_t    ts_unpack_packets(ts *, ts_packets *);
ssize_t    ts_unpack_stream(ts *, stream *);
ssize_t    ts_unpack_buffer(ts *, buffer *);

list      *ts_streams(ts *);
void       ts_debug(ts *);

#endif /* TS_H_INCLUDED */
