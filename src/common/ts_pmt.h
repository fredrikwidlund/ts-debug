#ifndef TS_PMT_H_INCLUDED
#define TS_PMT_H_INCLUDED

typedef struct ts_pmt_stream ts_pmt_stream;
struct ts_pmt_stream
{
  uint8_t  stream_type;
  unsigned elementary_pid:13;
};

typedef struct ts_pmt ts_pmt;
struct ts_pmt
{
  int      id;
  int      id_extension;
  int      version;
  unsigned pcr_pid:13;
  list     streams;
};

void    ts_pmt_construct(ts_pmt *);
ssize_t ts_pmt_construct_buffer(ts_pmt *, buffer *);
void    ts_pmt_destruct(ts_pmt *);
ssize_t ts_pmt_unpack_stream(ts_pmt *, stream *);
ssize_t ts_pmt_unpack_buffer(ts_pmt *, buffer *);
void    ts_pmt_debug(ts_pmt *, FILE *);

#endif /* TS_PMT_H_INCLUDED */
