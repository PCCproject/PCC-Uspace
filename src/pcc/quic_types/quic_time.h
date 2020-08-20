#ifndef QUIC_TIME_
#define QUIC_TIME_

#include <cmath>
#include <cstdint>
#include <limits>
#include <ostream>

class QuicTime {
 public:
  class Delta {
   public:
    explicit inline Delta(int64_t time_offset) : time_offset_(time_offset) {}

    static inline Delta Zero() { return Delta(0); }
    static inline Delta Infinite() {return Delta(kQuicInfiniteTimeUs);}

    static inline Delta FromSeconds(int64_t secs) {
      return Delta(secs * 1000 * 1000);
    }
    static inline Delta FromMilliseconds(int64_t ms) {return Delta(ms * 1000);}
    static inline Delta FromMicroseconds(int64_t us) {return Delta(us);}

    inline int64_t ToSeconds() const { return time_offset_ / 1000 / 1000; }
    inline int64_t ToMilliseconds() const { return time_offset_ / 1000; }
    inline int64_t ToMicroseconds() const { return time_offset_; }

    inline bool IsZero() const { return time_offset_ == 0; }
    inline bool IsInfinite() const {return time_offset_ == kQuicInfiniteTimeUs;}

   private:
    friend inline bool operator==(QuicTime::Delta lhs, QuicTime::Delta rhs);
    friend inline bool operator<(QuicTime::Delta lhs, QuicTime::Delta rhs);
    friend inline QuicTime::Delta operator<<(QuicTime::Delta lhs, size_t rhs);
    friend inline QuicTime::Delta operator>>(QuicTime::Delta lhs, size_t rhs);

    friend inline QuicTime::Delta operator+(QuicTime::Delta lhs,
                                            QuicTime::Delta rhs);
    friend inline QuicTime::Delta operator-(QuicTime::Delta lhs,
                                            QuicTime::Delta rhs);
    friend inline QuicTime::Delta operator*(QuicTime::Delta lhs, int rhs);
    friend inline QuicTime::Delta operator*(QuicTime::Delta lhs, double rhs);

    friend inline QuicTime operator+(QuicTime lhs, QuicTime::Delta rhs);
    friend inline QuicTime operator-(QuicTime lhs, QuicTime::Delta rhs);
    friend inline QuicTime::Delta operator-(QuicTime lhs, QuicTime rhs);

    static const int64_t kQuicInfiniteTimeUs =
        std::numeric_limits<int64_t>::max();

    int64_t time_offset_;
    friend class QuicTime;
  };

  static inline QuicTime Zero() { return QuicTime(0); }
  static inline QuicTime Infinite() {
    return QuicTime(Delta::kQuicInfiniteTimeUs);
  }

  inline bool IsInitialized() const { return 0 != time_; }

 private:
  friend inline bool operator==(QuicTime lhs, QuicTime rhs);
  friend inline bool operator<(QuicTime lhs, QuicTime rhs);
  friend inline QuicTime operator+(QuicTime lhs, QuicTime::Delta rhs);
  friend inline QuicTime operator-(QuicTime lhs, QuicTime::Delta rhs);
  friend inline QuicTime::Delta operator-(QuicTime lhs, QuicTime rhs);

  explicit inline QuicTime(int64_t time) : time_(time) {}

  int64_t time_;
};

// Non-member relational operators for QuicTime::Delta.
inline bool operator==(QuicTime::Delta lhs, QuicTime::Delta rhs) {
  return lhs.time_offset_ == rhs.time_offset_;
}
inline bool operator!=(QuicTime::Delta lhs, QuicTime::Delta rhs) {
  return !(lhs == rhs);
}
inline bool operator<(QuicTime::Delta lhs, QuicTime::Delta rhs) {
  return lhs.time_offset_ < rhs.time_offset_;
}
inline bool operator>(QuicTime::Delta lhs, QuicTime::Delta rhs) {
  return rhs < lhs;
}
inline bool operator<=(QuicTime::Delta lhs, QuicTime::Delta rhs) {
  return !(rhs < lhs);
}
inline bool operator>=(QuicTime::Delta lhs, QuicTime::Delta rhs) {
  return !(lhs < rhs);
}
inline QuicTime::Delta operator>>(QuicTime::Delta lhs, size_t rhs) {
  return QuicTime::Delta(lhs.time_offset_ >> rhs);
}

// Non-member relational operators for QuicTime.
inline bool operator==(QuicTime lhs, QuicTime rhs) {
  return lhs.time_ == rhs.time_;
}
inline bool operator!=(QuicTime lhs, QuicTime rhs) {
  return !(lhs == rhs);
}
inline bool operator<(QuicTime lhs, QuicTime rhs) {
  return lhs.time_ < rhs.time_;
}
inline bool operator>(QuicTime lhs, QuicTime rhs) {
  return rhs < lhs;
}
inline bool operator<=(QuicTime lhs, QuicTime rhs) {
  return !(rhs < lhs);
}
inline bool operator>=(QuicTime lhs, QuicTime rhs) {
  return !(lhs < rhs);
}

// Non-member arithmetic operators for QuicTime::Delta.
inline QuicTime::Delta operator+(QuicTime::Delta lhs, QuicTime::Delta rhs) {
  return QuicTime::Delta(lhs.time_offset_ + rhs.time_offset_);
}
inline QuicTime::Delta operator-(QuicTime::Delta lhs, QuicTime::Delta rhs) {
  return QuicTime::Delta(lhs.time_offset_ - rhs.time_offset_);
}
inline QuicTime::Delta operator*(QuicTime::Delta lhs, int rhs) {
  return QuicTime::Delta(lhs.time_offset_ * rhs);
}
inline QuicTime::Delta operator*(QuicTime::Delta lhs, double rhs) {
  return QuicTime::Delta(
      static_cast<int64_t>(std::llround(lhs.time_offset_ * rhs)));
}
inline QuicTime::Delta operator*(int lhs, QuicTime::Delta rhs) {
  return rhs * lhs;
}
inline QuicTime::Delta operator*(double lhs, QuicTime::Delta rhs) {
  return rhs * lhs;
}

// Non-member arithmetic operators for QuicTime and QuicTime::Delta.
inline QuicTime operator+(QuicTime lhs, QuicTime::Delta rhs) {
  return QuicTime(lhs.time_ + rhs.time_offset_);
}
inline QuicTime operator-(QuicTime lhs, QuicTime::Delta rhs) {
  return QuicTime(lhs.time_ - rhs.time_offset_);
}
inline QuicTime::Delta operator-(QuicTime lhs, QuicTime rhs) {
  return QuicTime::Delta(lhs.time_ - rhs.time_);
}

#endif
