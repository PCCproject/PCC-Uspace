#ifdef QUIC_PORT
#ifdef QUIC_PORT_LOCAL
#include "net/quic/core/congestion_control/pcc_utlity_calculator.h"
#else
#include "gfe/quic/core/congestion_control/pcc_utlity_calculator.h"
#endif
#else
#include "pcc_utlity_calculator.h"
#endif

#ifdef QUIC_PORT
#ifdef QUIC_PORT_LOCAL
namespace net {
#else
namespace gfe_quic {
#endif
#endif

#ifndef QUIC_PORT
//#define DEBUG_UTILITY_CALC
//#define DEBUG_MONITOR_INTERVAL_QUEUE_ACKS
//#define DEBUG_MONITOR_INTERVAL_QUEUE_LOSS
//#define DEBUG_INTERVAL_SIZE

#endif

namespace {
#ifndef QUIC_PORT_LOCAL
// Number of microseconds per second.
const float kNumMicrosPerSecond = 1000000.0f;
#endif
// An exponent in the utility function.
const size_t kBitsPerMegabit = 1024 * 1024;
}  // namespace

PccUtilityCalculator::PccUtilityCalculator() {}

PccUtilityCalculator::~PccUtilityCalculator() {}

float PccUtilityCalculator::CalculateUtility(const MonitorInterval& interval);

#ifdef QUIC_PORT
} // namespace gfe_quic
#endif
