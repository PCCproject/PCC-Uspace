#ifndef QUIC_TYPES_
#define QUIC_TYPES_

#include "quic_time.h"

#include <cstdint>
#include <vector>

typedef bool HasRetransmittableData;
static bool HAS_RETRANSMITTABLE_DATA = true;

typedef uint64_t QuicByteCount;
typedef uint64_t QuicPacketCount;
typedef uint64_t QuicPacketLength;
typedef uint64_t QuicPacketNumber;

struct AckedPacket {
  AckedPacket(QuicPacketNumber packet_number,
              QuicPacketLength bytes_acked,
              QuicTime receive_timestamp)
      : packet_number(packet_number),
        bytes_acked(bytes_acked),
        receive_timestamp(receive_timestamp) {}

  QuicPacketNumber packet_number;
  QuicPacketLength bytes_acked;
  QuicTime receive_timestamp;
};

struct LostPacket {
  LostPacket(QuicPacketNumber packet_number, QuicPacketLength bytes_lost)
      : packet_number(packet_number), bytes_lost(bytes_lost) {}

  QuicPacketNumber packet_number;
  QuicPacketLength bytes_lost;
};

typedef std::vector<AckedPacket> AckedPacketVector;
typedef std::vector<LostPacket> LostPacketVector;

#endif
