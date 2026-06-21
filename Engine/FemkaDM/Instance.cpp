/*
    SlateX - 2026
*/
#include "Instance.hpp"
#include <algorithm>
#include <sstream>

Instance::Instance(const std::string& ClassName)
    : m_className(ClassName)
    , m_name(ClassName) {
}

Instance::~Instance() {
    // с owning m_children (shared_ptr) деструктор и так не вызовется,
    // пока родитель нас держит — отдельная отвязка тут не нужна
}

// --- NetId registry ---
// статика на файл, не на класс — чтобы не тащить unordered_map в каждый .o,
// который инклюдит Instance.hpp

static std::unordered_map<uint32_t, InstanceWeak> g_netIdRegistry;

void Instance::SetNetId(uint32_t id) {
    // снимаем себя со старого id, если был
    if (m_netId != 0) {
        auto it = g_netIdRegistry.find(m_netId);
        if (it != g_netIdRegistry.end())
            g_netIdRegistry.erase(it);
    }

    m_netId = id;

    if (id != 0)
        g_netIdRegistry[id] = weak_from_this();
}

InstanceRef Instance::FindByNetId(uint32_t id) {
    auto it = g_netIdRegistry.find(id);
    if (it == g_netIdRegistry.end()) return nullptr;
    return it->second.lock();
}

// --- SetName ---

void Instance::SetName(const std::string& Name) {
    m_name = Name;
    NotifyChanged("Name");
}

// --- SetParent ---

void Instance::SetParent(InstanceRef Parent) {
    if (m_destroyed) return;

    auto OldParent = m_parent.lock();

    // detach from old parent
    if (OldParent)
        OldParent->RemoveChild(this);

    m_parent = Parent ? Parent : InstanceWeak{};

    // attach to new parent
    if (Parent)
        Parent->AddChild(shared_from_this());

    NotifyChanged("Parent");

    // fire ancestry changed on self and all descendants
    auto Self = shared_from_this();
    if (AncestryChanged) AncestryChanged(Self, Parent);
}

// --- AddChild / RemoveChild ---

void Instance::AddChild(InstanceRef Child) {
    m_children.push_back(Child);

    if (ChildAdded) ChildAdded(Child);
    if (OnChildAdded) OnChildAdded(Child); // Replicator: InstanceAdded по сети
    FireDescendantAdded(Child);
}

void Instance::RemoveChild(Instance* Child) {
    InstanceRef Removed;

    m_children.erase(
        std::remove_if(m_children.begin(), m_children.end(),
            [&](const InstanceRef& C) {
                if (C.get() == Child) {
                    Removed = C;
                    return true;
                }
                return false;
            }),
        m_children.end()
    );

    if (Removed) {
        FireDescendantRemoving(Removed);
        if (ChildRemoved) ChildRemoved(Removed);
        if (OnChildRemoved) OnChildRemoved(Removed); // Replicator: InstanceRemoved по сети
    }
}

// --- GetChildren ---

std::vector<InstanceRef> Instance::GetChildren() const {
    return m_children;
}

// --- GetDescendants ---

std::vector<InstanceRef> Instance::GetDescendants() const {
    std::vector<InstanceRef> Out;
    for (const auto& Child : m_children) {
        Out.push_back(Child);
        auto Sub = Child->GetDescendants();
        Out.insert(Out.end(), Sub.begin(), Sub.end());
    }
    return Out;
}

// --- FindFirstChild ---

InstanceRef Instance::FindFirstChild(const std::string& Name, bool Recursive) const {
    for (const auto& Child : m_children) {
        if (Child->m_name == Name) return Child;
        if (Recursive) {
            auto Found = Child->FindFirstChild(Name, true);
            if (Found) return Found;
        }
    }
    return nullptr;
}

// --- FindFirstChildOfClass ---

InstanceRef Instance::FindFirstChildOfClass(const std::string& ClassName) const {
    for (const auto& Child : m_children) {
        if (Child->m_className == ClassName) return Child;
    }
    return nullptr;
}

// --- FindFirstChildWhichIsA ---

InstanceRef Instance::FindFirstChildWhichIsA(const std::string& ClassName) const {
    for (const auto& Child : m_children) {
        if (Child->IsA(ClassName)) return Child;
    }
    return nullptr;
}

// --- FindFirstAncestor ---

InstanceRef Instance::FindFirstAncestor(const std::string& Name) const {
    auto Current = m_parent.lock();
    while (Current) {
        if (Current->m_name == Name) return Current;
        Current = Current->m_parent.lock();
    }
    return nullptr;
}

// --- FindFirstAncestorWhichIsA ---

InstanceRef Instance::FindFirstAncestorWhichIsA(const std::string& ClassName) const {
    auto Current = m_parent.lock();
    while (Current) {
        if (Current->IsA(ClassName)) return Current;
        Current = Current->m_parent.lock();
    }
    return nullptr;
}

// --- GetFullName ---

std::string Instance::GetFullName() const {
    auto Parent = m_parent.lock();
    if (!Parent) return m_name;
    return Parent->GetFullName() + "." + m_name;
}

// --- WaitForChild ---

InstanceRef Instance::WaitForChild(const std::string& Name, double TimeoutSec) {
    // for now just check if it already exists — scheduler integration comes later
    // da lazy version
    return FindFirstChild(Name);
}

// --- IsDescendantOf / IsAncestorOf ---

bool Instance::IsDescendantOf(const Instance* Ancestor) const {
    auto Current = m_parent.lock();
    while (Current) {
        if (Current.get() == Ancestor) return true;
        Current = Current->m_parent.lock();
    }
    return false;
}

bool Instance::IsAncestorOf(const Instance* Descendant) const {
    return Descendant->IsDescendantOf(this);
}

// --- IsA ---

bool Instance::IsA(const std::string& ClassName) const {
    // base implementation — only checks own class
    // subclasses override this to include their parents
    return m_className == ClassName || ClassName == "Instance";
}

// --- IsPropertyLocked / LockProperty / UnlockProperty ---
// см. Replicator::CanClientChangeProperty — залоченные свойства
// клиент не может менять напрямую, только сервер

bool Instance::IsPropertyLocked(const std::string& Prop) const {
    return std::find(m_lockedProps.begin(), m_lockedProps.end(), Prop) != m_lockedProps.end();
}

void Instance::LockProperty(const std::string& Prop) {
    if (!IsPropertyLocked(Prop))
        m_lockedProps.push_back(Prop);
}

void Instance::UnlockProperty(const std::string& Prop) {
    m_lockedProps.erase(
        std::remove(m_lockedProps.begin(), m_lockedProps.end(), Prop),
        m_lockedProps.end()
    );
}

// --- Destroy ---

void Instance::Destroy() {
    if (m_destroyed) return;
    m_destroyed = true;

    if (Destroying) Destroying();

    // нужно дёрнуть OnDestroyed (хук Replicator-а) до того как мы
    // отвалимся от shared_ptr-графа — иначе Replicator может не
    // успеть снять netId из своих карт
    if (OnDestroyed) OnDestroyed(shared_from_this());

    // nuke all children first
    auto Children = GetChildren();
    for (auto& Child : Children)
        Child->Destroy();

    // detach from parent
    auto Parent = m_parent.lock();
    if (Parent)
        Parent->RemoveChild(this);

    m_parent.reset();

    // disconnect all events so nothing fires anymore
    ChildAdded         = nullptr;
    ChildRemoved       = nullptr;
    DescendantAdded    = nullptr;
    DescendantRemoving = nullptr;
    AncestryChanged    = nullptr;
    Destroying         = nullptr;
    Changed            = nullptr;
    OnChanged          = nullptr;
    OnChildAdded       = nullptr;
    OnChildRemoved     = nullptr;
    OnDestroyed        = nullptr;
}

// --- Clone ---

InstanceRef Instance::Clone() const {
    if (!m_archivable) return nullptr;
    return CloneImpl();
}

InstanceRef Instance::CloneImpl() const {
    auto Copy = std::make_shared<Instance>(m_className);
    Copy->m_name       = m_name;
    Copy->m_archivable = m_archivable;
    Copy->m_filterMode = m_filterMode;
    Copy->m_lockedProps = m_lockedProps;

    for (const auto& Child : m_children) {
        if (!Child->m_archivable) continue;
        auto ChildCopy = Child->Clone();
        if (ChildCopy)
            ChildCopy->SetParent(Copy);
    }
    return Copy;
}

// --- ClearAllChildren ---

void Instance::ClearAllChildren() {
    auto Children = GetChildren();
    for (auto& Child : Children)
        Child->Destroy();
}

// --- NotifyChanged ---

void Instance::NotifyChanged(const std::string& PropName) {
    if (Changed)   Changed(PropName);
    if (OnChanged) OnChanged(PropName); // Replicator: InstanceChanged по сети
}

// --- FireDescendantAdded / FireDescendantRemoving ---

void Instance::FireDescendantAdded(InstanceRef Descendant) {
    if (DescendantAdded) DescendantAdded(Descendant);

    // propagate up the tree
    auto Parent = m_parent.lock();
    if (Parent) Parent->FireDescendantAdded(Descendant);
}

void Instance::FireDescendantRemoving(InstanceRef Descendant) {
    if (DescendantRemoving) DescendantRemoving(Descendant);

    auto Parent = m_parent.lock();
    if (Parent) Parent->FireDescendantRemoving(Descendant);
}