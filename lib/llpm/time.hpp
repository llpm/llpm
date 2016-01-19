#ifndef __LLPM_TIME_HPP__
#define __LLPM_TIME_HPP__

#include <util/macros.hpp>
#include <cassert>

namespace llpm {

/**
 * Quantity meta-data. How precise/accurate is the quantity? Is it an estimate
 * or a final value? Etc...
 */
class Quantity {
    /// Is this quantity valid (finite)? If not, why?
    enum Validity {
        _Unknown,
        _Variable,
        _Finite
    } _validity;

    /// Is this quantity an estimate which could later change or is it
    /// finalized an fixed?
    bool _estimate;

public:
    Quantity(Validity validity, bool estimate) :
        _validity(validity), _estimate(estimate) { }

    DEF_GET_NP(validity);
    DEF_GET_NP(estimate);

    bool valid() const {
        return _validity == _Finite;
    }

    bool unknown() const {
        return _validity == _Unknown;
    }

    bool variable() const {
        return _validity == _Variable;
    }
    
    bool finite() const {
        return _validity == _Finite;
    }

    bool fixed() const {
        return !_estimate;
    }

    static Quantity Unknown() {
        return Quantity(_Unknown, false);
    }

    static Quantity FiniteFixed() {
        return Quantity(_Finite, false);
    }

    static Quantity FiniteEstimate() {
        return Quantity(_Finite, true);
    }

    static Quantity Variable() {
        return Quantity(_Variable, false);
    }
};


/**
 * A utility class to indicate the number of pipeline stages in a particular
 * region of blocks.
 */
class PipelineDepth : public Quantity {
    /// The number of registers in a
    unsigned _registers;
    
public:
    PipelineDepth():
        Quantity(Quantity::Unknown())
    { }

    PipelineDepth(Quantity q) :
        Quantity(q) {
        assert(!q.valid());
    }

    PipelineDepth(const PipelineDepth& d) :
        Quantity(d),
        _registers(d._registers)
    { }

    PipelineDepth(Quantity qf, unsigned registers) :
        Quantity(qf),
        _registers(registers) { }

    static PipelineDepth Variable() {
        return PipelineDepth(Quantity::Variable(), 0);
    }

    static PipelineDepth Fixed(unsigned regs) {
        return PipelineDepth(Quantity::FiniteFixed(), regs);
    }

    unsigned registers() const {
        assert(valid());
        return _registers;
    }
};

/**
 * A utility class to help deal with quantities of real time (e.g. seconds).
 */
class Time : public Quantity {
    // Amount of time, in seconds
    double _time;

    Time(Quantity qf, double seconds) :
        Quantity(qf),
        _time(seconds)
    { }

    Time(double seconds, Quantity q) :
        Quantity(q),
        _time(seconds)
    { }

public:
    Time() :
        Quantity(Quantity::Unknown()),
        _time(0.0)
    { }

    Time(const Time& t) :
        Quantity(t),
        _time(t._time)
    { }

    Time(Quantity q) :
        Quantity(q) {
        assert(!q.valid());
    }

    static Time Variable() {
        return Time(Quantity::Variable(), 0);
    }

    double sec() const {
        return _time;
    }

    static Time s(double t, Quantity q = Quantity::FiniteEstimate()) {
        return Time(t, q);
    }
    static Time ms(double t, Quantity q = Quantity::FiniteEstimate()) {
        return Time(t * 1e-3, q);
    }
    static Time us(double t, Quantity q = Quantity::FiniteEstimate()) {
        return Time(t * 1e-6, q);
    }
    static Time ns(double t, Quantity q = Quantity::FiniteEstimate()) {
        return Time(t * 1e-9, q);
    }
    static Time ps(double t, Quantity q = Quantity::FiniteEstimate()) {
        return Time(t * 1e-12, q);
    }

    Time& operator+=(const Time& t) {
        _time += t._time;
        return *this;
    }
};

/**
 * Latency can be measured both by time and the number of pipeline stages.
 * Which is more important and how they should be used is application and
 * backend and operation dependent.
 */
class Latency {
    Time _time;
    PipelineDepth _depth;

public:
    Latency() :
        _time(),
        _depth()
    { }

    Latency(Time t, PipelineDepth d) :
        _time(t),
        _depth(d)
    { }

    Latency(PipelineDepth d) :
        _time(),
        _depth(d)
    { }

    Latency(Time t) :
        _time(t),
        _depth()
    { }

    Latency(const Latency& l) :
        _time(l._time),
        _depth(l._depth)
    { }

    DEF_GET_NP(time);
    DEF_GET_NP(depth);

};

///*********
///  Utility math functions 
///*********

inline Time operator+(const Time &t1, const Time &t2) {
    assert(t1.validity() == t2.validity());
    return Time::s(t1.sec() + t2.sec(), t1);
}

inline Time operator-(const Time &t1, const Time &t2) {
    assert(t1.validity() == t2.validity());
    return Time::s(t1.sec() - t2.sec(), t1);
}

inline Time operator*(const Time &t1, unsigned n) {
    return Time::s(t1.sec() * n, t1);
}

inline bool operator>(const Time &t1, const Time &t2) {
    assert(t1.valid() && t2.valid());
    return t1.sec() > t2.sec();
}

inline bool operator>=(const Time &t1, const Time &t2) {
    assert(t1.valid() && t2.valid());
    return t1.sec() >= t2.sec();
}

inline bool operator<(const Time &t1, const Time &t2) {
    assert(t1.valid() && t2.valid());
    return t1.sec() < t2.sec();
}

inline bool operator<=(const Time &t1, const Time &t2) {
    assert(t1.valid() && t2.valid());
    return t1.sec() <= t2.sec();
}

inline bool operator==(const Time &t1, const Time &t2) {
    if (t1.valid() && t2.valid())
        return t1.sec() == t2.sec() &&
               t1.validity() == t2.validity();
    return (t1.validity() == t2.validity());
}

inline PipelineDepth operator+(const PipelineDepth &t1, const PipelineDepth &t2) {
    assert(t1.validity() == t2.validity());
    return PipelineDepth(t1, t1.registers() + t2.registers());
}

inline PipelineDepth operator-(const PipelineDepth &t1, const PipelineDepth &t2) {
    assert(t1.validity() == t2.validity());
    return PipelineDepth(t1, t1.registers() - t2.registers());
}

inline PipelineDepth operator*(const PipelineDepth &t1, unsigned n) {
    return PipelineDepth::Fixed(t1.registers() * n);
}

inline bool operator>(const PipelineDepth &t1, const PipelineDepth &t2) {
    assert(t1.valid() && t2.valid());
    return t1.registers() > t2.registers();
}

inline bool operator>=(const PipelineDepth &t1, const PipelineDepth &t2) {
    assert(t1.valid() && t2.valid());
    return t1.registers() >= t2.registers();
}

inline bool operator<(const PipelineDepth &t1, const PipelineDepth &t2) {
    assert(t1.valid() && t2.valid());
    return t1.registers() < t2.registers();
}

inline bool operator<=(const PipelineDepth &t1, const PipelineDepth &t2) {
    assert(t1.valid() && t2.valid());
    return t1.registers() <= t2.registers();
}

inline bool operator==(const PipelineDepth &t1, const PipelineDepth &t2) {
    if (t1.valid() && t2.valid())
        return t1.registers() == t2.registers() &&
               t1.validity() == t2.validity();
    return (t1.validity() == t2.validity());
}

inline Latency operator+(const Latency& l1, const Latency& l2) {
    return Latency(l1.time() + l2.time(),
                   l1.depth() + l2.depth());
}

} // namespace llpm

#endif // __LLPM_TIME_HPP__
