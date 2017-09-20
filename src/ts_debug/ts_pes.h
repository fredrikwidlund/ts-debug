#ifndef TS_PES_H_INCLUDED
#define TS_PES_H_INCLUDED

typedef struct ts_pes ts_pes;
struct ts_pes
{
  int       id;
  unsigned  scrambling_control:2;
  unsigned  priority:1;
  unsigned  data_alignment_indicator:1;
  unsigned  copyright:1;
  unsigned  original:1;
  unsigned  has_pts:1;
  unsigned  has_dts:1;
  uint64_t  pts;
  uint64_t  dts;
  void     *payload_data;
  size_t    payload_size;
};

void    ts_pes_construct(ts_pes *);
void    ts_pes_destruct(ts_pes *);
ssize_t ts_pes_unpack(ts_pes *, void *, size_t);
void    ts_pes_debug(ts_pes *, FILE *, int);

#endif /* TS_PES_H_INCLUDED */
