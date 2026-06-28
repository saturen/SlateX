/*
    SlateX - 2026
*/
#pragma once
#include "PVInstance.hpp"
#include "../Reflection/Reflection.hpp"
#include "../Reflection/PropTraits.hpp"

// BasePart — простейший физический объект (куб/сфера/клин).
// Сравни с gen2 NetworkEvent.cpp: там на каждое свойство уходило
// ~15 строк лямбд. Здесь — одна строка через MakeProperty<>.
class BasePart : public PVInstance {
public:
    BasePart() : PVInstance("BasePart") {}

    CFrame GetCFrame() const { return m_cframe; }
    void   SetCFrame(CFrame v) { m_cframe = v; NotifyChanged("CFrame"); }

    Vector3 GetSize() const { return m_size; }
    void    SetSize(Vector3 v) { m_size = v; NotifyChanged("Size"); }

    bool GetAnchored() const { return m_anchored; }
    void SetAnchored(bool v) { m_anchored = v; NotifyChanged("Anchored"); }

    // --- PVInstance ---
    CFrame GetPivot() const override { return m_cframe; }
    void   PivotTo(const CFrame& v) override { SetCFrame(v); }

    bool IsA(const std::string& ClassName) const override {
        if (ClassName == "BasePart") return true;
        return PVInstance::IsA(ClassName);
    }

private:
    CFrame  m_cframe   = CFrame::Identity();
    Vector3 m_size     = Vector3(1, 1, 1);
    bool    m_anchored = false;
};

// --- регистрация свойств: компактно, без лямбд вручную ---
SLATE_CLASS_BEGIN(BasePart, PVInstance)
    SLATE_PROP(BasePart, "CFrame",   GetCFrame,   SetCFrame)
    SLATE_PROP(BasePart, "Size",     GetSize,     SetSize)
    SLATE_PROP(BasePart, "Anchored", GetAnchored, SetAnchored)
SLATE_CLASS_END(BasePart, PVInstance)