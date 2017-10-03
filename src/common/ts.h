#ifndef TS_H_INCLUDED
#define TS_H_INCLUDED

typedef struct ts ts;
struct ts
{
  list streams;
};

void       ts_construct(ts *);
void       ts_destruct(ts *);
ts_stream *ts_lookup(ts *, int);
ts_stream *ts_create(ts *, int);
ssize_t    ts_pack_packets(ts *, ts_packets *);
ssize_t    ts_pack(ts *, stream *);
ssize_t    ts_unpack_packets(ts *, ts_packets *);
ssize_t    ts_unpack(ts *, stream *);
list      *ts_streams(ts *);
void       ts_debug(ts *);

#endif /* TS_H_INCLUDED */
