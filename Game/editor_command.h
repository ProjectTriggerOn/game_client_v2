#pragma once
//=============================================================================
// editor_command.h — undoable edit commands + LIFO stack (spec §3.5, §8.2).
// Index-based commands are valid under strict LIFO undo/redo (see plan notes).
//=============================================================================
#include <memory>
#include <vector>
#include "editor_map.h"

namespace editor {

enum class ElemKind { Box, Collider, Model };

struct EditorCommand {
    virtual ~EditorCommand() = default;
    virtual void Do()   = 0;
    virtual void Undo() = 0;
};

class CommandStack {
public:
    void Execute(std::unique_ptr<EditorCommand> cmd);   // runs Do(), pushes, clears redo
    void Undo();
    void Redo();
    void Clear();
    bool CanUndo() const { return !m_Done.empty(); }
    bool CanRedo() const { return !m_Undone.empty(); }
private:
    std::vector<std::unique_ptr<EditorCommand>> m_Done;
    std::vector<std::unique_ptr<EditorCommand>> m_Undone;
};

// Remove one element; Undo reinserts it at the same index.
class DeleteCommand : public EditorCommand {
public:
    DeleteCommand(EditorMap& map, ElemKind kind, int index);
    void Do() override;
    void Undo() override;
private:
    EditorMap& m_Map; ElemKind m_Kind; int m_Index;
    PlacedBox      m_Box;
    EditorCollider m_Col;
    PlacedModel    m_Model;
};

// Set a transform field on Do (after), restore on Undo (before).
class MoveCommand : public EditorCommand {
public:
    MoveCommand(EditorMap& map, ElemKind kind, int index,
                const DirectX::XMFLOAT3& before, const DirectX::XMFLOAT3& after);
    void Do() override;   // for Collider, before/after are the delta applied to min&max
    void Undo() override;
private:
    void Apply(const DirectX::XMFLOAT3& posOrDelta, const DirectX::XMFLOAT3& other, bool useAfter);
    EditorMap& m_Map; ElemKind m_Kind; int m_Index;
    DirectX::XMFLOAT3 m_Before, m_After;
};

class ScaleCommand : public EditorCommand {
public:
    ScaleCommand(EditorMap& map, ElemKind kind, int index,
                 const DirectX::XMFLOAT3& beforeScaleOrMin, const DirectX::XMFLOAT3& beforeMax,
                 const DirectX::XMFLOAT3& afterScaleOrMin,  const DirectX::XMFLOAT3& afterMax);
    void Do() override;
    void Undo() override;
private:
    EditorMap& m_Map; ElemKind m_Kind; int m_Index;
    DirectX::XMFLOAT3 m_BeforeA, m_BeforeB, m_AfterA, m_AfterB;   // Box: A=scale (B unused); Collider: A=min,B=max
};

class RotateCommand : public EditorCommand {
public:
    RotateCommand(EditorMap& map, int boxOrModelIndex, ElemKind kind,
                  const DirectX::XMFLOAT3& beforeEuler, const DirectX::XMFLOAT3& afterEuler);
    void Do() override;
    void Undo() override;
private:
    EditorMap& m_Map; ElemKind m_Kind; int m_Index;
    DirectX::XMFLOAT3 m_Before, m_After;
};

// Append a new element; Undo pops it (must be the last element — enforced by LIFO).
class AddBoxCommand : public EditorCommand {
public:
    AddBoxCommand(EditorMap& map, const PlacedBox& box);
    void Do() override; void Undo() override;
    int AddedIndex() const { return m_Index; }
private:
    EditorMap& m_Map; PlacedBox m_Box; int m_Index = -1;
};

class AddModelCommand : public EditorCommand {
public:
    AddModelCommand(EditorMap& map, const PlacedModel& model, const EditorCollider& collider);
    void Do() override; void Undo() override;
    int AddedModelIndex() const { return m_ModelIndex; }
private:
    EditorMap& m_Map; PlacedModel m_Model; EditorCollider m_Col;
    int m_ModelIndex = -1; int m_ColIndex = -1;
};

} // namespace editor
