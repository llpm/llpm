#ifndef __LLPM_EXCEPTIONS_HPP__
#define __LLPM_EXCEPTIONS_HPP__

#include <string>
#include <exception>

namespace llpm {

class Exception {
public:
    std::string msg;
    Exception(std::string msg) : msg(msg) { }
    virtual const char* what() noexcept {
        return msg.c_str();
    }
};

class InvalidCall : public Exception {
public:
    InvalidCall(std::string msg = "") :
        Exception("Function/method call invalid. " + msg) { }
};

class ImplementationError : public Exception {
public:
    ImplementationError(std::string msg = "") :
        Exception("Internal error detected. " + msg) { }
};

class NullException: public Exception {
public:
    NullException(std::string msg = "") :
        Exception("NULL value not allowed here. " + msg) { }
};

} // namespace llpm

#endif // __LLPM_EXCEPTIONS_HPP__
