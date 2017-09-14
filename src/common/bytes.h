#ifndef BYTES_H_INCLUDED
#define BYTES_H_INCLUDED

typedef struct bytes bytes;
struct bytes
{
  uint8_t *base;
  uint8_t *end;
};

void      bytes_construct(bytes *, void *, size_t);
void      bytes_truncate(bytes *, size_t);
int       bytes_valid(bytes *);
size_t    bytes_size(bytes *);
void      bytes_read(bytes *, void **, size_t);
uint8_t   bytes_read1(bytes *);
uint16_t  bytes_read2(bytes *);
uint32_t  bytes_read4(bytes *);
uint64_t  bytes_read6(bytes *);
uint64_t  bytes_read8(bytes *);
uint64_t  bytes_bits(uint64_t, int, int, int);
uint8_t   bytes_index(bytes *, size_t);
void      bytes_rest(bytes *, void **, size_t *);
void     *bytes_data(bytes *);

#endif /* BYTES_H_INCLUDED */
