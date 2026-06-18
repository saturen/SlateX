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
    // make sure we cleaned up
    if (!m_destroyed)
        RemoveChild(this);
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
    // clean up dead weak ptrs while we are here
    m_children.erase(
        std::remove_if(m_children.begin(), m_children.end(),
            [](const InstanceWeak& W) { return W.expired(); }),
        m_children.end()
    );

    m_children.push_back(Child);

    if (ChildAdded) ChildAdded(Child);
    FireDescendantAdded(Child);
}

void Instance::RemoveChild(Instance* Child) {
    InstanceRef Removed;

    m_children.erase(
        std::remove_if(m_children.begin(), m_children.end(),
            [&](const InstanceWeak& W) {
                auto Locked = W.lock();
                if (Locked && Locked.get() == Child) {
                    Removed = Locked;
                    return true;
                }
                return W.expired();
            }),
        m_children.end()
    );

    if (Removed) {
        FireDescendantRemoving(Removed);
        if (ChildRemoved) ChildRemoved(Removed);
    }
}

// --- GetChildren ---

std::vector<InstanceRef> Instance::GetChildren() const {
    std::vector<InstanceRef> Out;
    for (const auto& W : m_children) {
        auto Locked = W.lock();
        if (Locked) Out.push_back(Locked);
    }
    return Out;
}

// --- GetDescendants ---

std::vector<InstanceRef> Instance::GetDescendants() const {
    std::vector<InstanceRef> Out;
    for (const auto& W : m_children) {
        auto Child = W.lock();
        if (!Child) continue;
        Out.push_back(Child);
        auto Sub = Child->GetDescendants();
        Out.insert(Out.end(), Sub.begin(), Sub.end());
    }
    return Out;
}

// --- FindFirstChild ---

InstanceRef Instance::FindFirstChild(const std::string& Name, bool Recursive) const {
    for (const auto& W : m_children) {
        auto Child = W.lock();
        if (!Child) continue;
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
    for (const auto& W : m_children) {
        auto Child = W.lock();
        if (Child && Child->m_className == ClassName) return Child;
    }
    return nullptr;
}

// --- FindFirstChildWhichIsA ---

InstanceRef Instance::FindFirstChildWhichIsA(const std::string& ClassName) const {
    for (const auto& W : m_children) {
        auto Child = W.lock();
        if (Child && Child->IsA(ClassName)) return Child;
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

// --- Destroy ---

void Instance::Destroy() {
    if (m_destroyed) return;
    m_destroyed = true;

    if (Destroying) Destroying();

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
    ChildAdded        = nullptr;
    ChildRemoved      = nullptr;
    DescendantAdded   = nullptr;
    DescendantRemoving = nullptr;
    AncestryChanged   = nullptr;
    Destroying        = nullptr;
    Changed           = nullptr;
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

    for (const auto& W : m_children) {
        auto Child = W.lock();
        if (!Child || !Child->m_archivable) continue;
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
    if (Changed) Changed(PropName);
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