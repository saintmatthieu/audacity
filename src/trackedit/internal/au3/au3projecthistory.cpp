/*
* Audacity: A Digital Audio Editor
*/

#include "au3projecthistory.h"

#include "UndoManager.h"
#include "au3wrap/iau3project.h"
#include "libraries/lib-project-history/ProjectHistory.h"
#include "libraries/lib-project/Project.h"

using namespace au::trackedit;
using namespace au::au3;

void au::trackedit::Au3ProjectHistory::init()
{
    auto* const project = projectPtr();
    IF_ASSERT_FAILED(project) {
        return;
    }
    ::ProjectHistory::Get(*project).InitialState();
}

bool au::trackedit::Au3ProjectHistory::undoAvailable()
{
    auto* const project = projectPtr();
    if (!project) {
        return false;
    }
    return ::ProjectHistory::Get(*project).UndoAvailable();
}

void au::trackedit::Au3ProjectHistory::undo()
{
    auto* const project = projectPtr();
    IF_ASSERT_FAILED(project) {
        return;
    }
    auto& undoManager = UndoManager::Get(*project);
    undoManager.Undo(
        [&]( const UndoStackElem& elem ){
        ::ProjectHistory::Get(*project).PopState(elem.state);
    });

    m_isUndoRedoAvailableChanged.notify();
}

bool au::trackedit::Au3ProjectHistory::redoAvailable()
{
    auto* const project = projectPtr();
    if (!project) {
        return false;
    }
    return ::ProjectHistory::Get(*project).RedoAvailable();
}

void au::trackedit::Au3ProjectHistory::redo()
{
    auto* const project = projectPtr();
    IF_ASSERT_FAILED(project) {
        return;
    }
    auto& undoManager = UndoManager::Get(*project);
    undoManager.Redo(
        [&]( const UndoStackElem& elem ){
        ::ProjectHistory::Get(*project).PopState(elem.state);
    });

    m_isUndoRedoAvailableChanged.notify();
}

void au::trackedit::Au3ProjectHistory::pushHistoryState(const std::string& longDescription, const std::string& shortDescription)
{
    auto* const project = projectPtr();
    IF_ASSERT_FAILED(project) {
        return;
    }
    ::ProjectHistory::Get(*project).PushState(TranslatableString { longDescription, {} }, TranslatableString { shortDescription, {} });

    m_isUndoRedoAvailableChanged.notify();
}

void Au3ProjectHistory::pushHistoryState(const std::string& longDescription, const std::string& shortDescription, UndoPushType flags)
{
    auto* const project = projectPtr();
    IF_ASSERT_FAILED(project) {
        return;
    }
    UndoPush undoFlags = static_cast<UndoPush>(flags);
    ::ProjectHistory::Get(*project).PushState(TranslatableString { longDescription, {} }, TranslatableString { shortDescription, {} },
                                              undoFlags);

    m_isUndoRedoAvailableChanged.notify();
}

void Au3ProjectHistory::modifyState(bool autoSave)
{
    if (!globalContext()->currentProject()) {
        return;
    }
    auto* const project = projectPtr();
    IF_ASSERT_FAILED(project) {
        return;
    }
    ::ProjectHistory::Get(*project).ModifyState(autoSave);
}

void Au3ProjectHistory::markUnsaved()
{
    auto* const project = projectPtr();
    if (!project) {
        return;
    }
    ::UndoManager::Get(*project).MarkUnsaved();
}

muse::async::Notification Au3ProjectHistory::isUndoRedoAvailableChanged() const
{
    return m_isUndoRedoAvailableChanged;
}

Au3Project* au::trackedit::Au3ProjectHistory::projectPtr()
{
    const auto project = globalContext()->currentProject();
    if (!project) {
        return nullptr;
    }
    return reinterpret_cast<Au3Project*>(project->au3ProjectPtr());
}
