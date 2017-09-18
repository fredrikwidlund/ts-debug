#ifndef TS_PSI_H_INCLUDED
#define TS_PSI_H_INCLUDED

typedef struct ts_psi ts_psi;
typedef struct ts_psi_pat ts_psi_pat;
typedef struct ts_psi_pmt ts_psi_pmt;
typedef struct ts_psi_pmt_stream ts_psi_pmt_stream;

struct ts_psi_pat
{
  int        present;
  int        id_extension;
  int        version;
  int        program_number;
  int        program_pid;
};

struct ts_psi_pmt_stream
{
  int        type;
  int        pid;
};

struct ts_psi_pmt
{
  int        present;
  int        id_extension;
  int        version;
  int        pcr_pid;
  list       streams;
};

struct ts_psi
{
  ts_psi_pat pat;
  ts_psi_pmt pmt;
};

void    ts_psi_construct(ts_psi *);
void    ts_psi_destruct(ts_psi *);
ssize_t ts_psi_unpack(ts_psi *, void *, size_t);

#endif /* TS_PSI_H_INCLUDED */
