#ifndef TS_STREAM_H_INCLUDED
#define TS_STREAM_H_INCLUDED

typedef struct ts_streams ts_streams;

enum ts_stream_type
{
  TS_STREAM_TYPE_UNKNOWN,
  TS_STREAM_TYPE_PSI,
  TS_STREAM_TYPE_PES
};

typedef struct ts_unit ts_unit;
typedef struct ts_stream ts_stream;

struct ts_unit
{
  unsigned    complete:1;
  unsigned    unpacked:1;
  ts_pes      pes;
  buffer      data;
};

struct ts_stream
{
  ts_streams *streams;
  int         pid;
  int         type;
  int         content_type;
  int         continuity_counter;
  list        units;
};

void       ts_unit_destruct(ts_unit *);
ts_unit   *ts_unit_new(void);
int        ts_unit_write(ts_unit *, ts_packet *);
void       ts_unit_debug(ts_unit *, FILE *, int);

void       ts_stream_construct(ts_stream *, ts_streams *, int, int, int);
void       ts_stream_destruct(ts_stream *);
int        ts_stream_process(ts_stream *, ts_unit *);
int        ts_stream_type(ts_stream *, int, int);
int        ts_stream_write(ts_stream *, ts_packet *);
ssize_t    ts_stream_pack(ts_stream *, ts_packets *);
void       ts_stream_debug(ts_stream *, FILE *, int);

#endif /* TS_STREAM_H_INCLUDED */
