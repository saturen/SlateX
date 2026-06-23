/*
    SlateX - 2026
*/
#pragma once
#include <cmath>
#include <string>
#include "Vector3.hpp"

#ifdef _WIN32
#define ENGINE_API __declspec(dllexport)
#else
#define ENGINE_API
#endif

// CFrame — pos + rotation (3x3 rotation matrix, row-major).
// R[0..2] = right, R[3..5] = up, R[6..8] = back
struct ENGINE_API CFrame {
    Vector3 Position;
    float   R[9] = { 1,0,0,  0,1,0,  0,0,1 }; // identity

    CFrame() = default;
    explicit CFrame(const Vector3& Pos) : Position(Pos) {}
    CFrame(const Vector3& Pos, const float Rot[9]) : Position(Pos) {
        for (int i = 0; i < 9; i++) R[i] = Rot[i];
    }

    static CFrame Identity() { return CFrame(); }

    // --- meow ---
    CFrame operator+(const Vector3& Delta) const {
        CFrame Out = *this;
        Out.Position = Position + Delta;
        return Out;
    }

    // --- composition (this * other) as 4x4 matrix ---
    CFrame operator*(const CFrame& o) const {
        CFrame Out;
        // R_out = R_this * R_other
        for (int row = 0; row < 3; row++) {
            for (int col = 0; col < 3; col++) {
                float sum = 0.f;
                for (int k = 0; k < 3; k++)
                    sum += R[row*3 + k] * o.R[k*3 + col];
                Out.R[row*3 + col] = sum;
            }
        }
        // pos_out = R_this * pos_other + pos_this
        Out.Position = PointToWorldSpace(o.Position);
        return Out;
    }

    // self explainatory
    Vector3 PointToWorldSpace(const Vector3& p) const {
        return Position + Vector3(
            R[0]*p.X + R[1]*p.Y + R[2]*p.Z,
            R[3]*p.X + R[4]*p.Y + R[5]*p.Z,
            R[6]*p.X + R[7]*p.Y + R[8]*p.Z
        );
    }

    // WorldSpace converter (WorldPos -> local, R^T bc R being funny)
    Vector3 PointToObjectSpace(const Vector3& p) const {
        Vector3 d = p - Position;
        return Vector3(
            R[0]*d.X + R[3]*d.Y + R[6]*d.Z,
            R[1]*d.X + R[4]*d.Y + R[7]*d.Z,
            R[2]*d.X + R[5]*d.Y + R[8]*d.Z
        );
    }

    bool operator==(const CFrame& o) const {
        if (!(Position == o.Position)) return false;
        for (int i = 0; i < 9; i++)
            if (R[i] != o.R[i]) return false;
        return true;
    }

    std::string ToString() const {
        return "CFrame(" + Position.ToString() + ")";
    }
};