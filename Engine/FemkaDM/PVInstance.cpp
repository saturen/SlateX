/*
    SlateX - 2026
*/
#include "PVInstance.hpp"
#include "../Reflection/Reflection.hpp"

// abstract (pure virtual GetPivot/PivotTo), no factory possible — but
// BasePart's (and one day Model's) sol::bases<PVInstance>() still needs
// dis usertype to exist, and da Reflection chain-walk needs it registered
// too, see SLATE_ABSTRACT_CLASS_END. No SLATE_PROP lines needed, PVInstance
// has no plain-value properties of its own.
SLATE_CLASS_BEGIN(PVInstance, Instance)
SLATE_ABSTRACT_CLASS_END(PVInstance, Instance)