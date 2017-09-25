#ifndef TS_PACKET_H_INCLUDED
#define TS_PACKET_H_INCLUDED

typedef struct ts_packet ts_packet;

typedef struct ts_adaptation_field ts_adaptation_field;
struct ts_adaptation_field
{
  int x;
};

struct ts_packet
{
  void                *data;
  size_t               size;
  unsigned             transport_error_indicator:1;
  unsigned             payload_unit_start_indicator:1;
  unsigned             transport_priority:1;
  unsigned             pid:13;
  unsigned             transport_scrambling_control:2;
  unsigned             adaptation_field_control:2;
  unsigned             continuity_counter:4;
  ts_adaptation_field  adaptation_field;
  void                *payload_data;
  size_t               payload_size;
};

void    ts_packet_construct(ts_packet *);
void    ts_packet_destruct(ts_packet *);
ssize_t ts_packet_unpack(ts_packet *, void *, size_t);

#endif /* TS_PACKET_H_INCLUDED */
