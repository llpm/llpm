#ifndef __LLPM_UTIL_TIME_HPP__
#define __LLPM_UTIL_TIME_HPP__

namespace llpm {

class Time {
    // Amount of time, in seconds
    double _time;

    Time(double seconds) :
        _time(seconds)
    { }

public:
    Time() :
        _time(0.0)
    { }

    Time(const Time& t) :
        _time(t._time)
    { }

    double sec() const {
        return _time;
    }

    static Time s(double t) {
        return Time(t);
    }
    static Time ms(double t) {
        return Time(t * 1e-3);
    }
    static Time us(double t) {
        return Time(t * 1e-6);
    }
    static Time ns(double t) {
        return Time(t * 1e-9);
    }
    static Time ps(double t) {
        return Time(t * 1e-12);
    }

    Time& operator+=(const Time& t) {
        _time += t._time;
        return *this;
    }
};

inline Time operator+(const Time &t1, const Time &t2) {
    return Time::s(t1.sec() + t2.sec());
}

inline Time operator-(const Time &t1, const Time &t2) {
    return Time::s(t1.sec() - t2.sec());
}

inline Time operator*(const Time &t1, unsigned n) {
    return Time::s(t1.sec() * n);
}

inline bool operator>(const Time &t1, const Time &t2) {
    return t1.sec() > t2.sec();
}

inline bool operator>=(const Time &t1, const Time &t2) {
    return t1.sec() >= t2.sec();
}

inline bool operator<(const Time &t1, const Time &t2) {
    return t1.sec() < t2.sec();
}

inline bool operator<=(const Time &t1, const Time &t2) {
    return t1.sec() <= t2.sec();
}

inline bool operator==(const Time &t1, const Time &t2) {
    return t1.sec() == t2.sec();
}

} // namespace llpm

#endif // __LLPM_UTIL_TIME_HPP__
