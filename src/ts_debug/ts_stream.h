#ifndef TS_STREAM_H_INCLUDED
#define TS_STREAM_H_INCLUDED

enum ts_unit_type
{
  TS_UNIT_TYPE_UNKNOWN,
  TS_UNIT_TYPE_PSI,
  TS_UNIT_TYPE_PES
};

typedef struct ts_unit ts_unit;
struct ts_unit
{
  int    type;
  buffer data;
};

typedef struct ts_stream ts_stream;
struct ts_stream
{
  int    pid;
  int    continuity_counter;
  int    unit_type;
  list   units;
};

typedef struct ts_streams ts_streams;
struct ts_streams
{
  list   streams;
};

void       ts_unit_destruct(ts_unit *);
ts_unit   *ts_unit_new(int);
void       ts_unit_type(ts_unit *, int);
int        ts_unit_type_guess(ts_unit *);
int        ts_unit_write(ts_unit *, ts_packet *);
void       ts_unit_debug(ts_unit *, FILE *, int);

void       ts_stream_destruct(ts_stream *);
ts_stream *ts_stream_new(int);
void       ts_stream_type(ts_stream *, int);
int        ts_stream_write(ts_stream *, ts_packet *);
void       ts_stream_debug(ts_stream *, FILE *, int);

void       ts_streams_construct(ts_streams *);
void       ts_streams_destruct(ts_streams *);
ts_stream *ts_streams_lookup(ts_streams *, int);
int        ts_streams_write(ts_streams *, ts_packets *);
void       ts_streams_debug(ts_streams *, FILE *, int);

#endif /* TS_STREAM_H_INCLUDED */
