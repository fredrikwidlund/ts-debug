#ifndef TS_PES_H_INCLUDED
#define TS_PES_H_INCLUDED

typedef struct ts_pes ts_pes;
struct ts_pes
{
  int id;
};

void    ts_pes_construct(ts_pes *);
void    ts_pes_destruct(ts_pes *);
ssize_t ts_pes_unpack(ts_pes *, void *, size_t);

#endif /* TS_PES_H_INCLUDED */
