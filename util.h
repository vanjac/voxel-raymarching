#ifndef UTIL_H
#define UTIL_H

class noncopyable {
protected:
    noncopyable() = default;
    ~noncopyable() = default;

    noncopyable(noncopyable const &) = delete;
    void operator=(noncopyable const &) = delete;
    noncopyable(noncopyable &&) = default;
    noncopyable &operator=(noncopyable &&) = default;
};

#endif // UTIL_H
