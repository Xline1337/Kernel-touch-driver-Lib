#include <math.h>

#ifndef GAMEHELPER_VECTORSTRUCT_H
#define GAMEHELPER_VECTORSTRUCT_H

struct Vector2 {
    float x;
    float y;

    inline Vector2() : x(0), y(0) {};

    inline Vector2(const float x, const float y) : x(x), y(y) {};


    inline Vector2 operator+(const Vector2 &other) const {
        return Vector2(x + other.x, y + other.y);
    }

    inline Vector2 operator+(const float other) const {
        return Vector2(x + other, y + other);
    }

    inline Vector2 operator-(const Vector2 &other) const {
        return Vector2(x - other.x, y - other.y);
    }

    inline Vector2 operator-(const float other) const {
        return Vector2(x - other, y - other);
    }

    inline Vector2 operator*(const Vector2 &other) const {
        return Vector2(x * other.x, y * other.y);
    }

    inline Vector2 operator*(const float value) const {
        return Vector2(x * value, y * value);
    }

    inline Vector2 operator/(const Vector2 &other) const {
        if (other.x != 0 && other.y != 0) {
            return Vector2(x / other.x, y / other.y);
        }
        return Vector2();
    }

    inline Vector2 operator/(const float value) const {
        if (value != 0) {
            return Vector2(x / value, y / value);
        }
        return Vector2();
    }

    inline Vector2 operator-() const {
        return Vector2(-x, -y);
    }

    inline Vector2 &operator+=(const Vector2 &other) {
        x += other.x;
        y += other.y;
        return *this;
    }


    inline Vector2 &operator-=(const Vector2 &other) {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    inline Vector2 &operator+=(const float value) {
        x += value;
        y += value;
        return *this;
    }

    inline Vector2 &operator-=(const float value) {
        x -= value;
        y -= value;
        return *this;
    }

    inline Vector2 &operator*=(const float value) {
        x *= value;
        y *= value;
        return *this;
    }

    inline Vector2 &operator*=(const Vector2 &other) {
        x *= other.x;
        y *= other.y;
        return *this;
    }

    inline Vector2 &operator/=(const float &value) {
        x /= value;
        y /= value;
        return *this;
    }

    inline Vector2 &operator=(const Vector2 &other) {
        x = other.x;
        y = other.y;
        return *this;
    }

    inline bool operator==(const Vector2 &other) const {
        return x == other.x && y == other.y;
    }

    inline bool operator!=(const Vector2 &other) const {
        return x != other.x || y != other.y;
    }

    inline float operator[](int index) const {
        return (&x)[index];
    }

    inline float &operator[](int index) {
        return (&x)[index];
    }

    inline bool NotHaveZero() {
        return x != 0 && y != 0;
    }

    inline void zero() {
        x = y = 0;
    }

    inline float length() {
        return sqrt(x * x + y * y);
    }

};

#endif