/*
    SlateX - 2026
*/
#pragma once
#include "Instance.hpp"

// DataModel — корень дерева. Синглтон, держит "game".
// Дальше сюда добавятся Workspace/Players/ReplicatedStorage как дети.
class DataModel : public Instance {
public:
    static InstanceRef Get();

    bool IsA(const std::string& ClassName) const override {
        if (ClassName == "DataModel") return true;
        return Instance::IsA(ClassName);
    }

private:
    DataModel() : Instance("DataModel") { SetName("game"); }
};