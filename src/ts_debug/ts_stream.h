#ifndef TS_STREAM_H_INCLUDED
#define TS_STREAM_H_INCLUDED

enum ts_stream_type
{
  TS_STREAM_TYPE_UNKNOWN,
  TS_STREAM_TYPE_PSI,
  TS_STREAM_TYPE_PES
};

typedef struct ts_unit ts_unit;
typedef struct ts_stream ts_stream;
typedef struct ts_streams ts_streams;

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

struct ts_streams
{
  ts_psi      psi;
  list        streams;
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
void       ts_stream_debug(ts_stream *, FILE *, int);

void       ts_streams_construct(ts_streams *);
void       ts_streams_destruct(ts_streams *);
ts_stream *ts_streams_create(ts_streams *, int, int, int);
ts_stream *ts_streams_lookup(ts_streams *, int);
int        ts_streams_write(ts_streams *, ts_packets *);
void       ts_streams_debug(ts_streams *, FILE *, int);

#endif /* TS_STREAM_H_INCLUDED */
