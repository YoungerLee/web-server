#ifndef __FYLEE_NONCOPYABLE_H__
#define __FYLEE_NONCOPYABLE_H__

namespace fylee {

class Noncopyable {
public:
    Noncopyable(const Noncopyable&) = delete;

    Noncopyable& operator=(const Noncopyable&) = delete;

protected:
    Noncopyable() = default;

    ~Noncopyable() = default;

};

}

#endif
