#ifndef TS_PAT_H_INCLUDED
#define TS_PAT_H_INCLUDED

typedef struct ts_pat ts_pat;
struct ts_pat
{
  int      id;
  int      id_extension;
  int      version;
  uint16_t program_number;
  unsigned program_pid:13;
};

void    ts_pat_construct(ts_pat *);
ssize_t ts_pat_construct_buffer(ts_pat *, buffer *);
void    ts_pat_destruct(ts_pat *);
ssize_t ts_pat_unpack_stream(ts_pat *, stream *);
ssize_t ts_pat_unpack_buffer(ts_pat *, buffer *);
void    ts_pat_debug(ts_pat *, FILE *);

#endif /* TS_PAT_H_INCLUDED */
