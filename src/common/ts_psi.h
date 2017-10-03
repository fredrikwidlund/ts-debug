#ifndef TS_PSI_H_INCLUDED
#define TS_PSI_H_INCLUDED

typedef struct ts_psi ts_psi;
struct ts_psi
{
  uint8_t  id;
  unsigned section_length:10;
  uint16_t id_extension;
  unsigned version:5;
  unsigned current:1;
  uint8_t  section_number;
  uint8_t  last_section_number;
};

void    ts_psi_construct(ts_psi *);
void    ts_psi_destruct(ts_psi *);
ssize_t ts_psi_unpack_stream(ts_psi *, stream *);
ssize_t ts_psi_pointer_unpack(stream *);

#endif /* TS_PSI_H_INCLUDED */
