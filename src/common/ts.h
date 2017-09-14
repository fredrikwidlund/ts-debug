#ifndef TS_H_INCLUDED
#define TS_H_INCLUDED

enum ts_flag
{
  TS_FLAG_EOF              = 0x01
};

enum ts_private_data_flag
{
  TS_PRIVATE_DATA_FLAG_EBP = 0x01,
};

enum ts_af_flag
{
  TS_AF_FLAG_DI            = 0x80,
  TS_AF_FLAG_RAI           = 0x40,
  TS_AF_FLAG_ESPI          = 0x20,
  TS_AF_FLAG_PCR           = 0x10,
  TS_AF_FLAG_OPCR          = 0x08,
  TS_AF_FLAG_SPF           = 0x04,
  TS_AF_FLAG_TPDF          = 0x02,
  TS_AF_FLAG_AFEF          = 0x01
};

typedef struct ts_private_data ts_private_data;
struct ts_private_data
{
  uint8_t          flags;
  uint64_t         ebp_timestamp;
};

typedef struct ts_af ts_af;
struct ts_af
{
  uint8_t          flags;
  uint64_t         pcr;
  uint64_t         opcr;
  uint8_t          splicing_countdown;
  ts_private_data  private_data;
};

typedef struct ts_header ts_header;
struct ts_header
{
  unsigned         transport_error_indicator:1;
  unsigned         payload_unit_start_indicator:1;
  unsigned         transport_priority:1;
  unsigned         pid:13;
  unsigned         transport_scrambling_control:2;
  unsigned         adaptation_field_control:2;
  unsigned         continuity_counter:4;
  ts_af            adaptation_field;
};

typedef struct ts_frame ts_frame;
struct ts_frame
{
  ts_header        header;
  void            *data;
  size_t           size;
};

typedef struct ts ts;
struct ts
{
  int              flags;
  buffer           input;
  list             frames;
};

void      ts_construct(ts *);
void      ts_destruct(ts *);
int       ts_data(ts *, void *, size_t);
ts_frame *ts_read(ts *);
void      ts_frame_debug(FILE *, ts_frame *);
void      ts_frame_release(ts_frame *);

#endif /* TS_H_INCLUDED */
