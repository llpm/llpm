#pragma once

#include <string>
#include <exception>
#include <errno.h>
#include <string.h>

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
        Exception("Implementation error detected. " + msg) { }
};

class InternalError: public Exception {
public:
    InternalError(std::string msg = "") :
        Exception("Internal error detected. " + msg) { }
};

class NullException: public Exception {
public:
    NullException(std::string msg = "") :
        Exception("NULL value not allowed here. " + msg) { }
};

class InvalidArgument: public Exception {
public:
    InvalidArgument(std::string msg = "") :
        Exception("Invalid argument. " + msg) { }
};

class TypeError: public Exception {
public:
    TypeError(std::string msg = "") :
        Exception("Type error." + msg) { }
};

class IncompatibleException: public Exception {
public:
    IncompatibleException(std::string msg = "") :
        Exception("An incompatibility was encountered." + msg) { }
};

class SysError: public Exception {
public:
    SysError(std::string action = "doing something unspecified") :
        Exception("Error while " + action + ": " + std::string(strerror(errno))) { } 
};

class ExternalError: public Exception {
public:
    ExternalError(std::string msg = "") :
        Exception("External error: " + msg) { }
};



} // namespace llpm

