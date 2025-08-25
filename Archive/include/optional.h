#ifndef OPTIONAL_H
#define OPTIONAL_H

#include <utility>

template<typename T>
class Optional {
private:
    alignas(T) unsigned char storage[sizeof(T)];
    bool hasValue;

public:
    Optional() : hasValue(false) {}

    Optional(const T& value) : hasValue(true) {
        new (storage) T(value);
    }

    Optional(const Optional& other) : hasValue(other.hasValue) {
        if (hasValue) {
            new (storage) T(*other.ptr());
        }
    }

    Optional(Optional&& other) noexcept : hasValue(other.hasValue) {
        if (hasValue) {
            new (storage) T(std::move(*other.ptr()));
            other.hasValue = false;
        }
    }

    ~Optional() {
        if (hasValue) {
            ptr()->~T();
        }
    }

    Optional& operator=(const Optional& other) {
        if (this != &other) {
            if (hasValue) {
                ptr()->~T();
            }
            hasValue = other.hasValue;
            if (hasValue) {
                new (storage) T(*other.ptr());
            }
        }
        return *this;
    }

    bool has_value() const { return hasValue; }

    T& value() { return *ptr(); }
    const T& value() const { return *ptr(); }

    T* operator->() { return ptr(); }
    const T* operator->() const { return ptr(); }

    T& operator*() { return *ptr(); }
    const T& operator*() const { return *ptr(); }

private:
    T* ptr() { return reinterpret_cast<T*>(storage); }
    const T* ptr() const { return reinterpret_cast<const T*>(storage); }
};

#endif
