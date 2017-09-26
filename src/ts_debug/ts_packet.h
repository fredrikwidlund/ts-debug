#ifndef TS_PACKET_H_INCLUDED
#define TS_PACKET_H_INCLUDED

typedef struct ts_packet ts_packet;

typedef struct ts_adaptation_field ts_adaptation_field;
struct ts_adaptation_field
{
  unsigned             discontinuity_indicator:1;
  unsigned             random_access_indicator:1;
  unsigned             elementary_stream_priority_indicator:1;
  unsigned             pcr_flag:1;
  unsigned             opcr_flag:1;
  unsigned             splicing_point_flag:1;
  unsigned             transport_private_data_flag:1;
  unsigned             adaptation_field_extension_flag:1;
  uint64_t             pcr;
  uint64_t             opcr;
  uint8_t              splicing_countdown;
};

struct ts_packet
{
  unsigned             transport_error_indicator:1;
  unsigned             payload_unit_start_indicator:1;
  unsigned             transport_priority:1;
  unsigned             pid:13;
  unsigned             transport_scrambling_control:2;
  unsigned             adaptation_field_control:2;
  unsigned             continuity_counter:4;
  ts_adaptation_field  adaptation_field;
  void                *data;
  size_t               size;
};

void    ts_packet_construct(ts_packet *);
void    ts_packet_destruct(ts_packet *);
ssize_t ts_packet_unpack(ts_packet *, void *, size_t);

#endif /* TS_PACKET_H_INCLUDED */
