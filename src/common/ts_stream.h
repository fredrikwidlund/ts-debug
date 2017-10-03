#ifndef TS_STREAM_H_INCLUDED
#define TS_STREAM_H_INCLUDED

enum ts_stream_type
{
  TS_STREAM_TYPE_UNDEFINED,
  TS_STREAM_TYPE_PSI,
  TS_STREAM_TYPE_PES
};

typedef struct ts_unit ts_unit;
struct ts_unit
{
  unsigned  random_access_indicator:1;
  buffer    data;
};

typedef struct ts_stream ts_stream;
struct ts_stream
{
  int       pid;
  int       type;
  uint8_t   id;
  list      units;
};

void     ts_unit_construct(ts_unit *);
void     ts_unit_destruct(ts_unit *);
void     ts_unit_append(ts_unit *, void *, size_t);

void     ts_stream_construct(ts_stream *, int);
void     ts_stream_destruct(ts_stream *);
void     ts_stream_type(ts_stream *, int, uint8_t);
list    *ts_stream_units(ts_stream *);
ts_unit *ts_stream_read_unit(ts_stream *);
ssize_t  ts_stream_unpack(ts_stream *, ts_packet *);
ssize_t  ts_stream_pack(ts_stream *, ts_packets *);

#endif /* TS_STREAM_H_INCLUDED */
