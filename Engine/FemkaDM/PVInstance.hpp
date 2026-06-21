/*
    SlateX - 2026
*/
#pragma once
#include "Instance.hpp"
#include "../Math/Vector3.hpp"
#include "../Math/CFrame.hpp"

// PVInstance — слой для всего, что имеет положение/ориентацию в мире.
// BasePart и Model оба наследуют отсюда, чтобы у обоих был единый
// интерфейс перемещения (GetPivot/PivotTo), независимо от того,
// состоит объект из одной части или из иерархии.
class PVInstance : public Instance {
public:
    explicit PVInstance(const std::string& ClassName) : Instance(ClassName) {}

    // единая точка опоры — для BasePart это её CFrame,
    // для Model — это её настраиваемый/вычисляемый pivot
    virtual CFrame GetPivot() const = 0;
    virtual void   PivotTo(const CFrame& NewPivot) = 0;

    // сдвигает текущий pivot на Delta, не трогая остальное
    void TranslateBy(const Vector3& Delta) {
        CFrame p = GetPivot();
        PivotTo(p + Delta);
    }

    bool IsA(const std::string& ClassName) const override {
        if (ClassName == "PVInstance") return true;
        return Instance::IsA(ClassName);
    }
};