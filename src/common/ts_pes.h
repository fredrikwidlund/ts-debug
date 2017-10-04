#ifndef TS_PES_H_INCLUDED
#define TS_PES_H_INCLUDED

typedef struct ts_pes ts_pes;
struct ts_pes
{
  uint8_t   stream_id;
  unsigned  scrambling_control:2;
  unsigned  priority:1;
  unsigned  data_alignment_indicator:1;
  unsigned  copyright:1;
  unsigned  original_or_copy:1;
  unsigned  pts_indicator:1;
  unsigned  dts_indicator:1;
  uint64_t  pts;
  uint64_t  dts;
  void     *data;
  size_t    size;
};

void    ts_pes_construct(ts_pes *);
ssize_t ts_pes_construct_buffer(ts_pes *, buffer *);
void    ts_pes_destruct(ts_pes *);
ssize_t ts_pes_unpack_stream(ts_pes *, stream *);
ssize_t ts_pes_unpack_buffer(ts_pes *, buffer *);
void    ts_pes_debug(ts_pes *, FILE *);

#endif /* TS_PES_H_INCLUDED */
