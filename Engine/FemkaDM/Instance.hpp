/*
    SlateX - 2026
*/
#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "../slDBG/slDBG.hpp"

class Instance;
using InstanceRef = std::shared_ptr<Instance>;
using InstanceWeak = std::weak_ptr<Instance>;

// base class for everything in the datamodel tree
// da fuck you doing, dont instantiate this directly
class Instance : public std::enable_shared_from_this<Instance> {
public:
    explicit Instance(const std::string& ClassName);
    virtual ~Instance();

    // --- properties ---

    const std::string& GetName()      const { return m_name; }
    const std::string& GetClassName() const { return m_className; }
    bool               GetArchivable() const { return m_archivable; }

    void SetName(const std::string& Name);
    void SetParent(InstanceRef Parent);
    void SetArchivable(bool Archivable) { m_archivable = Archivable; }

    InstanceRef GetParent() const { return m_parent.lock(); }

    // --- tree traversal ---

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

    // full path like "Workspace.Model.Part"
    std::string GetFullName() const;

    // waits until child with given name shows up
    // returns nullptr if it sucks and times out (timeoutSec <= 0 means wait forever)
    InstanceRef WaitForChild(const std::string& Name, double TimeoutSec = 0.0);

    // --- hierarchy checks ---

    bool IsDescendantOf(const Instance* Ancestor) const;
    bool IsAncestorOf(const Instance* Descendant) const;

    // this is the big one — checks class hierarchy
    // returns true if this IS a ClassName or inherits from it
    virtual bool IsA(const std::string& ClassName) const;

    // --- lifecycle ---

    // nukes this instance and all children, sets parent to nil
    // da proper way to delete things
    void Destroy();

    // deep copy of this instance and all archivable descendants
    InstanceRef Clone() const;

    // destroys all children, leaves this instance alive
    void ClearAllChildren();

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

protected:
    std::string  m_className;
    std::string  m_name;
    bool         m_archivable = true;
    bool         m_destroyed  = false;

    InstanceWeak              m_parent;
    std::vector<InstanceWeak> m_children;

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