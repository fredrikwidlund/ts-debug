#ifndef TS_PACKETS_H_INCLUDED
#define TS_PACKETS_H_INCLUDED

typedef struct ts_packets ts_packets;

struct ts_packets
{
  buffer               in;
  list                 packets;
};

void       ts_packets_construct(ts_packets *);
void       ts_packets_destruct(ts_packets *);
ssize_t    ts_packets_unpack(ts_packets *, void *, size_t);
ssize_t    ts_packets_pack(ts_packets *, void **, size_t *);
ts_packet *ts_packets_read(ts_packets *);

#endif /* TS_PACKETS_H_INCLUDED */