//=============================================================================
// editor_command.cpp
//=============================================================================
// Includes editor_command.h -> editor_map.h -> map_io.h (std::fopen); silence
// C4996 (promoted to error by /sdl), per the plan's global constraints.
#define _CRT_SECURE_NO_WARNINGS

#include "editor_command.h"

using namespace DirectX;

namespace editor {

void CommandStack::Execute(std::unique_ptr<EditorCommand> cmd) {
    cmd->Do();
    m_Done.push_back(std::move(cmd));
    m_Undone.clear();
}
void CommandStack::Undo() {
    if (m_Done.empty()) return;
    m_Done.back()->Undo();
    m_Undone.push_back(std::move(m_Done.back()));
    m_Done.pop_back();
}
void CommandStack::Redo() {
    if (m_Undone.empty()) return;
    m_Undone.back()->Do();
    m_Done.push_back(std::move(m_Undone.back()));
    m_Undone.pop_back();
}
void CommandStack::Clear() { m_Done.clear(); m_Undone.clear(); }

// ---- DeleteCommand ----
DeleteCommand::DeleteCommand(EditorMap& map, ElemKind kind, int index)
    : m_Map(map), m_Kind(kind), m_Index(index) {
    if (kind == ElemKind::Box)      m_Box   = map.boxes[index];
    else if (kind == ElemKind::Collider) m_Col = map.colliders[index];
    else                            m_Model = map.models[index];
}
void DeleteCommand::Do() {
    if (m_Kind == ElemKind::Box)      m_Map.boxes.erase(m_Map.boxes.begin() + m_Index);
    else if (m_Kind == ElemKind::Collider) m_Map.colliders.erase(m_Map.colliders.begin() + m_Index);
    else                              m_Map.models.erase(m_Map.models.begin() + m_Index);
}
void DeleteCommand::Undo() {
    if (m_Kind == ElemKind::Box)      m_Map.boxes.insert(m_Map.boxes.begin() + m_Index, m_Box);
    else if (m_Kind == ElemKind::Collider) m_Map.colliders.insert(m_Map.colliders.begin() + m_Index, m_Col);
    else                              m_Map.models.insert(m_Map.models.begin() + m_Index, m_Model);
}

// ---- MoveCommand ----
MoveCommand::MoveCommand(EditorMap& map, ElemKind kind, int index,
                         const XMFLOAT3& before, const XMFLOAT3& after)
    : m_Map(map), m_Kind(kind), m_Index(index), m_Before(before), m_After(after) {
    // Colliders track their min corner in Before/After; capture the current
    // size once so Do()/Undo() can set min+max ABSOLUTELY. A relative "max += d"
    // is not idempotent: Execute() runs Do() when the live-apply drag has
    // already moved the collider, so a delta re-add would double-count and
    // corrupt max (and undo couldn't restore it).
    if (kind == ElemKind::Collider) {
        const auto& c = map.colliders[index];
        m_Size = { c.max.x - c.min.x, c.max.y - c.min.y, c.max.z - c.min.z };
    }
}
void MoveCommand::Do() {
    if (m_Kind == ElemKind::Box)        m_Map.boxes[m_Index].pos = m_After;
    else if (m_Kind == ElemKind::Model) m_Map.models[m_Index].pos = m_After;
    else { auto& c = m_Map.colliders[m_Index];   // Collider: absolute min + preserved size
        c.min = m_After;
        c.max = { m_After.x + m_Size.x, m_After.y + m_Size.y, m_After.z + m_Size.z }; }
}
void MoveCommand::Undo() {
    if (m_Kind == ElemKind::Box)        m_Map.boxes[m_Index].pos = m_Before;
    else if (m_Kind == ElemKind::Model) m_Map.models[m_Index].pos = m_Before;
    else { auto& c = m_Map.colliders[m_Index];
        c.min = m_Before;
        c.max = { m_Before.x + m_Size.x, m_Before.y + m_Size.y, m_Before.z + m_Size.z }; }
}
void MoveCommand::Apply(const XMFLOAT3&, const XMFLOAT3&, bool) {}   // unused helper stub

// ---- ScaleCommand ----
ScaleCommand::ScaleCommand(EditorMap& map, ElemKind kind, int index,
                           const XMFLOAT3& bA, const XMFLOAT3& bB, const XMFLOAT3& aA, const XMFLOAT3& aB)
    : m_Map(map), m_Kind(kind), m_Index(index), m_BeforeA(bA), m_BeforeB(bB), m_AfterA(aA), m_AfterB(aB) {}
void ScaleCommand::Do() {
    if (m_Kind == ElemKind::Box)      m_Map.boxes[m_Index].scale = m_AfterA;
    else if (m_Kind == ElemKind::Collider) { m_Map.colliders[m_Index].min = m_AfterA; m_Map.colliders[m_Index].max = m_AfterB; }
    else                              m_Map.models[m_Index].scale = m_AfterA;
}
void ScaleCommand::Undo() {
    if (m_Kind == ElemKind::Box)      m_Map.boxes[m_Index].scale = m_BeforeA;
    else if (m_Kind == ElemKind::Collider) { m_Map.colliders[m_Index].min = m_BeforeA; m_Map.colliders[m_Index].max = m_BeforeB; }
    else                              m_Map.models[m_Index].scale = m_BeforeA;
}

// ---- RotateCommand ----
RotateCommand::RotateCommand(EditorMap& map, int index, ElemKind kind,
                             const XMFLOAT3& before, const XMFLOAT3& after)
    : m_Map(map), m_Kind(kind), m_Index(index), m_Before(before), m_After(after) {}
void RotateCommand::Do() {
    if (m_Kind == ElemKind::Box)        m_Map.boxes[m_Index].rotEuler = m_After;
    else if (m_Kind == ElemKind::Model) m_Map.models[m_Index].rotEuler = m_After;
}
void RotateCommand::Undo() {
    if (m_Kind == ElemKind::Box)        m_Map.boxes[m_Index].rotEuler = m_Before;
    else if (m_Kind == ElemKind::Model) m_Map.models[m_Index].rotEuler = m_Before;
}

// ---- AddBoxCommand ----
AddBoxCommand::AddBoxCommand(EditorMap& map, const PlacedBox& box) : m_Map(map), m_Box(box) {}
void AddBoxCommand::Do()   { m_Index = (int)m_Map.boxes.size(); m_Map.boxes.push_back(m_Box); }
void AddBoxCommand::Undo() { m_Map.boxes.pop_back(); }

// ---- AddModelCommand ----
AddModelCommand::AddModelCommand(EditorMap& map, const PlacedModel& model, const EditorCollider& collider)
    : m_Map(map), m_Model(model), m_Col(collider) {}
void AddModelCommand::Do() {
    m_ModelIndex = (int)m_Map.models.size(); m_Map.models.push_back(m_Model);
    m_ColIndex   = (int)m_Map.colliders.size(); m_Map.colliders.push_back(m_Col);
}
void AddModelCommand::Undo() { m_Map.colliders.pop_back(); m_Map.models.pop_back(); }

} // namespace editor
