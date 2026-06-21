/*
    SlateX - 2026
    All rights belong to someone idk lol
*/
#pragma once
#include <cmath>
#include <string>

#ifdef _WIN32
#define ENGINE_API __declspec(dllexport)
#else
#define ENGINE_API
#endif

struct ENGINE_API Vector3 {
    float X = 0, Y = 0, Z = 0;

    Vector3() = default;
    Vector3(float x, float y, float z) : X(x), Y(y), Z(z) {}

    // --- Арифметика ---
    Vector3 operator+(const Vector3& o) const { return {X+o.X, Y+o.Y, Z+o.Z}; }
    Vector3 operator-(const Vector3& o) const { return {X-o.X, Y-o.Y, Z-o.Z}; }
    Vector3 operator*(const Vector3& o) const { return {X*o.X, Y*o.Y, Z*o.Z}; }
    Vector3 operator/(const Vector3& o) const { return {X/o.X, Y/o.Y, Z/o.Z}; }
    Vector3 operator*(float s)          const { return {X*s,   Y*s,   Z*s  }; }
    Vector3 operator/(float s)          const { return {X/s,   Y/s,   Z/s  }; }
    Vector3 operator-()                 const { return {-X,    -Y,    -Z   }; }

    bool operator==(const Vector3& o) const {
        return X == o.X && Y == o.Y && Z == o.Z;
    }

    // --- Методы ---
    float Magnitude() const {
        return std::sqrt(X*X + Y*Y + Z*Z);
    }

    Vector3 Normalized() const {
        float m = Magnitude();
        if (m < 1e-6f) return {0, 0, 0};
        return {X/m, Y/m, Z/m};
    }

    float Dot(const Vector3& o) const {
        return X*o.X + Y*o.Y + Z*o.Z;
    }

    Vector3 Cross(const Vector3& o) const {
        return {
            Y*o.Z - Z*o.Y,
            Z*o.X - X*o.Z,
            X*o.Y - Y*o.X
        };
    }

    float Distance(const Vector3& o) const {
        return (*this - o).Magnitude();
    }

    std::string ToString() const {
        return "Vector3(" + std::to_string(X) + ", "
                          + std::to_string(Y) + ", "
                          + std::to_string(Z) + ")";
    }

    // --- Константы ---
    static Vector3 Zero()  { return {0, 0, 0}; }
    static Vector3 One()   { return {1, 1, 1}; }
    static Vector3 XAxis() { return {1, 0, 0}; }
    static Vector3 YAxis() { return {0, 1, 0}; }
    static Vector3 ZAxis() { return {0, 0, 1}; }
};

// Скаляр * Vector3
inline Vector3 operator*(float s, const Vector3& v) {
    return {s*v.X, s*v.Y, s*v.Z};
}