#ifndef TS_ADAPTATION_FIELD
#define TS_ADAPTATION_FIELD

typedef struct ts_adaptation_field ts_adaptation_field;

struct ts_adaptation_field
{
  unsigned  discontinuity_indicator:1;
  unsigned  random_access_indicator:1;
  unsigned  elementary_stream_priority_indicator:1;
  unsigned  pcr_flag:1;
  unsigned  opcr_flag:1;
  unsigned  splicing_point_flag:1;
  unsigned  transport_private_data_flag:1;
  unsigned  adaptation_field_extension_flag:1;
  uint64_t  pcr;
  uint64_t  opcr;
  uint8_t   splice_countdown;
  void     *private_data;
  uint8_t   private_size;
};

void    ts_adaptation_field_construct(ts_adaptation_field *);
void    ts_adaptation_field_destruct(ts_adaptation_field *);
size_t  ts_adaptation_field_size(ts_adaptation_field *);
ssize_t ts_adaptation_field_pack(ts_adaptation_field *, stream *, size_t);
ssize_t ts_adaptation_field_unpack(ts_adaptation_field *, stream *);

#endif /* TS_ADAPTATION_FIELD */
