#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <dynamic.h>

#include "bytes.h"
#include "ts.h"

/* internals */

static void ts_private_data_debug(FILE *f, ts_private_data *pd)
{
  double t;

  if (pd->flags & TS_PRIVATE_DATA_FLAG_EBP)
    {
      t = (pd->ebp_timestamp >> 32) + ((double)(pd->ebp_timestamp & UINT32_MAX) / (1ULL << 32));
      (void) fprintf(f, "[ts ebp marker] %f\n", t);
    }
}

static int ts_private_data_parse(ts_private_data *pd, void *data, size_t size)
{
  bytes b;
  int v, tag;
  char *id;

  bytes_construct(&b, data, size);
  tag = bytes_read1(&b);
  (void) bytes_read1(&b);
  bytes_read(&b, (void **) &id, 4);
  if (tag != 0xdf || strncmp(id, "EBP0", 4))
    return 0;

  pd->flags |= TS_PRIVATE_DATA_FLAG_EBP;

  v = bytes_read1(&b);
  if (v & 0x01)
    bytes_read(&b, NULL, 1);
  if (v & 0x20)
    bytes_read(&b, NULL, 1);
  if (v & 0x10)
    bytes_read(&b, NULL, 2);
  if (v & 0x08)
    pd->ebp_timestamp = bytes_read8(&b);

  return bytes_valid(&b) ? 0 : -1;
}

static void ts_af_debug(FILE *f, ts_af *af)
{
  (void) fprintf(f, "[ts adaptation field] flags %02x", af->flags);
  if (af->flags & TS_AF_FLAG_PCR)
    (void) fprintf(f, ", pcr %f", af->pcr / 27000000.);
  if (af->flags & TS_AF_FLAG_OPCR)
    (void) fprintf(f, ", opcr %f", af->opcr / 27000000.);
  (void) fprintf(f, "\n");

  if (af->flags & TS_AF_FLAG_TPDF)
    ts_private_data_debug(f, &af->private_data);
}

static int ts_af_parse(ts_af *af, void *data, size_t size)
{
  bytes b;
  uint64_t v;
  void *pd_data;
  size_t pd_size;
  int e;

  if (!data)
    return -1;

  if (!size)
    return 0;

  bytes_construct(&b, data, size);
  af->flags = bytes_read1(&b);
  if (af->flags & TS_AF_FLAG_PCR)
    {
      v = bytes_read6(&b);
      af->pcr = bytes_bits(v, 48, 0, 33) * 300 + bytes_bits(v, 48, 39, 9);
    }
  if (af->flags & TS_AF_FLAG_OPCR)
    {
      v = bytes_read6(&b);
      af->opcr = bytes_bits(v, 48, 0, 33) * 300 + bytes_bits(v, 48, 39, 9);
    }
  if (af->flags & TS_AF_FLAG_SPF)
    af->splicing_countdown = bytes_read1(&b);

  if (af->flags & TS_AF_FLAG_TPDF)
    {
      pd_size = bytes_read1(&b);
      bytes_read(&b, &pd_data, pd_size);
      e = ts_private_data_parse(&af->private_data, pd_data, pd_size);
      if (e == -1)
        return -1;
    }

  return bytes_valid(&b) ? 0 : -1;
}

static void ts_header_debug(FILE *f, ts_header *header)
{
  (void) fprintf(f, "[ts] tei %d, pusi %d, tp %d, pid %d, tsc %d, afc %d, cc %d\n",
                 header->transport_error_indicator, header->payload_unit_start_indicator,
                 header->transport_priority, header->pid, header->transport_scrambling_control,
                 header->adaptation_field_control, header->continuity_counter);

  if (header->adaptation_field_control & 0x02)
    ts_af_debug(f, &header->adaptation_field);
}

static int ts_header_parse(ts_header *header, void *data, size_t size)
{
  bytes b;
  int v, e;
  void *af_data;
  size_t af_size;

  bytes_construct(&b, data, size);
  v = bytes_read4(&b);
  if (bytes_bits(v, 32, 0, 8) != 'G')
    return -1;

  header->transport_error_indicator = bytes_bits(v, 32, 8, 1);
  header->payload_unit_start_indicator = bytes_bits(v, 32, 9, 1);
  header->transport_priority = bytes_bits(v, 32, 10, 1);
  header->pid = bytes_bits(v, 32, 11, 13);
  header->transport_scrambling_control = bytes_bits(v, 32, 24, 2);
  header->adaptation_field_control = bytes_bits(v, 32, 26, 2);
  header->continuity_counter = bytes_bits(v, 32, 28, 4);

  if (header->adaptation_field_control & 0x02)
    {
      af_size = bytes_read1(&b);
      bytes_read(&b, &af_data, af_size);
      e = ts_af_parse(&header->adaptation_field, af_data, af_size);
      if (e == -1)
        return -1;
    }

  return bytes_valid(&b) ? 0 : -1;
}

static ts_frame *ts_frame_new(void *data, size_t size)
{
  ts_frame *f;
  int e;

  f = malloc(sizeof *f);
  if (!f)
    abort();
  *f = (ts_frame) {0};

  f->size = size;
  f->data = malloc(f->size);
  if (!f->data)
    abort();
  memcpy(f->data, data, f->size);

  e = ts_header_parse(&f->header, f->data, f->size);
  if (e == -1)
    {
      ts_frame_release(f);
      return NULL;
    }

  return f;
}

static void ts_frames_release(void *object)
{
  ts_frame *f = *(ts_frame **) object;

  ts_frame_release(f);
}

/* ts */

void ts_construct(ts *ts)
{
  list_construct(&ts->frames);
}

void ts_destruct(ts *ts)
{
  buffer_destruct(&ts->input);
  list_destruct(&ts->frames, ts_frames_release);
}

int ts_data(ts *ts, void *data, size_t size)
{
  ts_frame *f;
  char *p;
  size_t n;

  if (size)
    buffer_insert(&ts->input, buffer_size(&ts->input), data, size);
  else
    ts->flags |= TS_FLAG_EOF;

  p = buffer_data(&ts->input);
  n = buffer_size(&ts->input);
  for (; n >= 188; p += 188, n -= 188)
    {
      f = ts_frame_new(p, 188);
      if (!f)
        return -1;
      list_push_back(&ts->frames, &f, sizeof f);
    }

  buffer_erase(&ts->input, 0, buffer_size(&ts->input) - n);
  return 0;
}

ts_frame *ts_read(ts *ts)
{
  ts_frame **f;

  f = list_front(&ts->frames);
  if (f == list_end(&ts->frames))
    return NULL;

  list_erase(f, NULL);
  return *f;
}

void ts_frame_debug(FILE *f, ts_frame *frame)
{
  ts_header_debug(f, &frame->header);
}

void ts_frame_release(ts_frame *frame)
{
  free(frame->data);
  free(frame);
}
