#ifndef OPTIONAL_MOVE_H
#define OPTIONAL_MOVE_H

#include <utility>
#include <type_traits>

// Optional class that supports move-only types
template<typename T>
class OptionalMove {
private:
    alignas(T) unsigned char storage[sizeof(T)];
    bool hasValue;
    
public:
    OptionalMove() : hasValue(false) {}
    
    // Constructor from value (copy)
    template<typename U = T,
             typename = typename std::enable_if<std::is_copy_constructible<U>::value>::type>
    OptionalMove(const T& value) : hasValue(true) {
        new (storage) T(value);
    }
    
    // Constructor from value (move)
    OptionalMove(T&& value) : hasValue(true) {
        new (storage) T(std::move(value));
    }
    
    // Move constructor
    OptionalMove(OptionalMove&& other) noexcept : hasValue(other.hasValue) {
        if (hasValue) {
            new (storage) T(std::move(*other.ptr()));
            other.hasValue = false;
        }
    }
    
    // Copy constructor (only if T is copyable)
    template<typename U = T,
             typename = typename std::enable_if<std::is_copy_constructible<U>::value>::type>
    OptionalMove(const OptionalMove& other) : hasValue(other.hasValue) {
        if (hasValue) {
            new (storage) T(*other.ptr());
        }
    }
    
    ~OptionalMove() {
        if (hasValue) {
            ptr()->~T();
        }
    }
    
    // Move assignment
    OptionalMove& operator=(OptionalMove&& other) noexcept {
        if (this != &other) {
            if (hasValue) {
                ptr()->~T();
            }
            hasValue = other.hasValue;
            if (hasValue) {
                new (storage) T(std::move(*other.ptr()));
                other.hasValue = false;
            }
        }
        return *this;
    }
    
    // Copy assignment (only if T is copyable)
    template<typename U = T,
             typename = typename std::enable_if<std::is_copy_constructible<U>::value>::type>
    OptionalMove& operator=(const OptionalMove& other) {
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
    
    void reset() {
        if (hasValue) {
            ptr()->~T();
            hasValue = false;
        }
    }
    
private:
    T* ptr() { return reinterpret_cast<T*>(storage); }
    const T* ptr() const { return reinterpret_cast<const T*>(storage); }
};

#endif // OPTIONAL_MOVE_H