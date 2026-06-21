/*
    SlateX - 2026
*/
#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cstdint>
#include <unordered_map>

class Instance;
using InstanceRef  = std::shared_ptr<Instance>;
using InstanceWeak = std::weak_ptr<Instance>;

enum class FilterMode : uint32_t {
    Server = 0,  // не реплицируется клиентам вообще
    Shared = 1,  // реплицируется всем, изменяем с обеих сторон (с проверкой ownership)
    Client = 2,  // реплицируется всем, но "истина" живёт на клиенте-владельце
};

// base class for everything in the datamodel tree
// da fuck you doing, dont instantiate this directly
class Instance : public std::enable_shared_from_this<Instance> {
public:
    explicit Instance(const std::string& ClassName);
    virtual ~Instance();

    // --- properties ---

    const std::string& GetClassName() const { return m_className; }
    const std::string& GetName()      const { return m_name; }
    void                SetName(const std::string& Name);

    // virtual IsA — переопределяется в каждом наследнике, проверяет всю цепочку
    virtual bool IsA(const std::string& ClassName) const;

    // --- сеть ---

    uint32_t   GetNetId() const { return m_netId; }
    void       SetNetId(uint32_t id);

    // обратный поиск — нужен для десериализации InstanceRef-аргументов
    // в NetworkEvent (см. Network/Shared/LuaArgSerializer)
    static InstanceRef FindByNetId(uint32_t id);
    FilterMode GetFilterMode() const { return m_filterMode; }
    void       SetFilterMode(FilterMode m) { m_filterMode = m; }

    // --- tree traversal ---

    InstanceRef GetParent() const { return m_parent.lock(); }
    void        SetParent(InstanceRef NewParent);

    // returns direct children
    std::vector<InstanceRef> GetChildren() const;

    // returns all descendants recursively
    std::vector<InstanceRef> GetDescendants() const;

    // finds direct child by name, recursive if you ask nicely
    InstanceRef FindFirstChild(const std::string& Name, bool Recursive = false) const;

    // finds first child whose ClassName matches exactly
    InstanceRef FindFirstChildOfClass(const std::string& ClassName) const;

    // finds first child that IsA given class
    InstanceRef FindFirstChildWhichIsA(const std::string& ClassName) const;

    // walks up the tree looking for ancestor by name
    InstanceRef FindFirstAncestor(const std::string& Name) const;
    InstanceRef FindFirstAncestorWhichIsA(const std::string& ClassName) const;

    // full path like "Workspace.Model.Part"
    std::string GetFullName() const;

    // waits until child with given name shows up
    // returns nullptr if it sucks and times out (timeoutSec <= 0 means wait forever)
    InstanceRef WaitForChild(const std::string& Name, double TimeoutSec = 0.0);

    // --- hierarchy checks ---

    bool IsDescendantOf(const Instance* Ancestor) const;
    bool IsAncestorOf(const Instance* Descendant) const;

    // --- lifecycle ---

    // nukes this instance and all children, sets parent to nil
    // da proper way to delete things
    void Destroy();

    // deep copy of this instance and all archivable descendants
    InstanceRef Clone() const;

    // destroys all children, leaves this instance alive
    void ClearAllChildren();

    bool IsDestroyed() const { return m_destroyed; }

    // --- свойства: блокировка от записи по сети (см. Replicator::CanClientChangeProperty) ---

    bool IsPropertyLocked(const std::string& Prop) const;
    void LockProperty(const std::string& Prop);
    void UnlockProperty(const std::string& Prop);

    // --- children management (internal, not really for scripts) ---

    void AddChild(InstanceRef Child);
    void RemoveChild(Instance* Child);

    // --- events ---

    // fires when a direct child is added
    std::function<void(InstanceRef)> ChildAdded;

    // fires when a direct child is removed
    std::function<void(InstanceRef)> ChildRemoved;

    // fires when any descendant is added
    std::function<void(InstanceRef)> DescendantAdded;

    // fires when any descendant is about to be removed
    std::function<void(InstanceRef)> DescendantRemoving;

    // fires when parent or any ancestor changes
    std::function<void(InstanceRef, InstanceRef)> AncestryChanged;

    // fires right before Destroy() nukes this instance
    std::function<void()> Destroying;

    // fires when any property changes — gives you the property name
    std::function<void(const std::string&)> Changed;

    // --- хуки для Replicator (заполняются им же при WatchInstance) ---

    std::function<void(const std::string&)> OnChanged;
    std::function<void(InstanceRef)>        OnChildAdded;
    std::function<void(InstanceRef)>        OnChildRemoved;
    std::function<void(InstanceRef)>        OnDestroyed;

protected:
    std::string m_className;
    std::string m_name;
    uint32_t    m_netId      = 0;
    FilterMode  m_filterMode = FilterMode::Shared;
    bool        m_archivable = true;
    bool        m_destroyed  = false;

    InstanceWeak              m_parent;
    std::vector<InstanceRef>  m_children;
    std::vector<std::string>  m_lockedProps;

    // called by subclasses to fire Changed event
    void NotifyChanged(const std::string& PropName);

    // deep copy implementation — subclasses override to copy their own props
    virtual InstanceRef CloneImpl() const;

private:
    // fires DescendantAdded up the ancestor chain
    void FireDescendantAdded(InstanceRef Descendant);

    // fires DescendantRemoving up the ancestor chain
    void FireDescendantRemoving(InstanceRef Descendant);
};