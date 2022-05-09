//
// Copyright (c) 2017-2020 the rbfx project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#pragma once

#include "../Core/ResourceDragDropPayload.h"
#include "../Project/EditorTab.h"
#include "../Project/ProjectEditor.h"

#include <Urho3D/Core/Signal.h>
#include <Urho3D/Utility/FileSystemReflection.h>

#include <EASTL/functional.h>
#include <EASTL/optional.h>

namespace Urho3D
{

void Foundation_ResourceBrowserTab(Context* context, ProjectEditor* projectEditor);

class ResourceBrowserFactory : public Object
{
    URHO3D_OBJECT(ResourceBrowserFactory, Object);

public:
    using Callback = ea::function<void(const ea::string& fileName, const ea::string& resourceName)>;

    ResourceBrowserFactory(Context* context,
        int group, const ea::string& title, const ea::string& fileName);
    ResourceBrowserFactory(Context* context,
        int group, const ea::string& title, const ea::string& fileName, const Callback& callback);

    /// Overridable interface
    /// @{
    virtual bool IsEnabled(const FileSystemEntry& parentEntry) const { return true; }
    virtual void BeginCreate() {}
    virtual void UpdateAndRender() {}
    virtual void EndCreate(const ea::string& fileName, const ea::string& resourceName);
    /// @}

    int GetGroup() const { return group_; }
    const ea::string& GetTitle() const { return title_; }
    const ea::string& GetFileName() const { return fileName_; }

    static bool Compare(const SharedPtr<ResourceBrowserFactory>& lhs, const SharedPtr<ResourceBrowserFactory>& rhs);

private:
    const int group_{};
    const ea::string title_;
    const ea::string fileName_;

    Callback callback_;
};

class ResourceBrowserTab : public EditorTab
{
    URHO3D_OBJECT(ResourceBrowserTab, EditorTab);

public:
    explicit ResourceBrowserTab(Context* context);
    ~ResourceBrowserTab() override;

    void AddFactory(SharedPtr<ResourceBrowserFactory> factory);

    /// Commands
    /// @{
    void DeleteSelected();
    void RenameSelected();
    void RevealInExplorerSelected();
    /// @}

    /// Implement EditorTab
    /// @{
    void WriteIniSettings(ImGuiTextBuffer* output) override;
    void ReadIniSettings(const char* line) override;
    /// @}

protected:
    /// Implement EditorTab
    /// @{
    void UpdateAndRenderContent() override;
    void ApplyHotkeys(HotkeyManager* hotkeyManager) override;
    /// @}

private:
    /// Root index and resource name used to safely reference an entry.
    struct EntryReference
    {
        unsigned rootIndex_{};
        ea::string resourcePath_;
    };

    struct ResourceRoot
    {
        ea::string name_;
        bool openByDefault_{};
        bool supportCompositeFiles_{};
        StringVector watchedDirectories_;
        ea::string activeDirectory_;

        SharedPtr<FileSystemReflection> reflection_;
    };

    void InitializeRoots();
    void InitializeDefaultFactories();
    void InitializeHotkeys();

    /// Render left panel
    /// @{
    void RenderDirectoryTree(const FileSystemEntry& entry, const ea::string& displayedName);
    /// @}

    /// Render right panel
    /// @{
    void RenderDirectoryContent();
    void RenderDirectoryUp(const FileSystemEntry& entry);
    void RenderDirectoryContentEntry(const FileSystemEntry& entry);
    void RenderCompositeFile(const FileSystemEntry& entry);
    void RenderCompositeFileEntry(const FileSystemEntry& entry, const FileSystemEntry& ownerEntry);
    /// @}

    /// Common rendering
    /// @{
    void UpdateAndRenderDialogs();

    void RenderEntryContextMenu(const FileSystemEntry& entry);
    ea::optional<unsigned> RenderEntryCreateContextMenu(const FileSystemEntry& entry);

    void RenderRenameDialog();
    void RenderDeleteDialog();
    void RenderCreateDialog();
    /// @}

    /// Drag&drop handling
    /// @{
    SharedPtr<ResourceDragDropPayload> CreateDragDropPayload(const FileSystemEntry& entry) const;
    void BeginEntryDrag(const FileSystemEntry& entry);
    void DropPayloadToFolder(const FileSystemEntry& entry);
    /// @}

    /// Utility functions
    /// @{
    ea::string GetEntryIcon(const FileSystemEntry& entry) const;
    unsigned GetRootIndex(const FileSystemEntry& entry) const;
    const ResourceRoot& GetRoot(const FileSystemEntry& entry) const;
    bool IsEntryFromCache(const FileSystemEntry& entry) const;
    EntryReference GetReference(const FileSystemEntry& entry) const;
    const FileSystemEntry* GetEntry(const EntryReference& ref) const;
    const FileSystemEntry* GetSelectedEntryForCursor() const;
    ea::pair<bool, ea::string> CheckFileNameInput(const FileSystemEntry& parentEntry,
        const ea::string& oldName, const ea::string& newName) const;
    /// @}

    /// Manage selection
    /// @{
    void SelectLeftPanel(const ea::string& path, ea::optional<unsigned> rootIndex = ea::nullopt);
    void SelectRightPanel(const ea::string& path);
    void AdjustSelectionOnRename(const ea::string& oldResourceName, const ea::string& newResourceName);
    void ScrollToSelection();
    /// @}

    /// Manipulation utilities and helpers
    /// @{
    void BeginEntryDelete(const FileSystemEntry& entry);
    void BeginEntryRename(const FileSystemEntry& entry);
    void BeginEntryCreate(const FileSystemEntry& entry, ResourceBrowserFactory* factory);

    void RefreshContents();
    void RevealInExplorer(const ea::string& path);
    void RenameEntry(const FileSystemEntry& entry, const ea::string& newName);
    void RenameOrMoveEntry(const ea::string& oldFileName, const ea::string& newFileName,
        const ea::string& oldResourceName, const ea::string& newResourceName, bool adjustSelection);
    void DeleteEntry(const FileSystemEntry& entry);
    void CleanupResourceCache(const ea::string& resourceName);
    void OpenEntryInEditor(const FileSystemEntry& entry);
    /// @}

    ea::vector<ResourceRoot> roots_;
    bool waitingForUpdate_{};

    ea::vector<SharedPtr<ResourceBrowserFactory>> factories_;
    bool sortFactories_{true};

    /// UI state
    /// @{
    struct LeftPanel
    {
        unsigned selectedRoot_{1};
        ea::string selectedPath_;

        bool scrollToSelection_{};
    } left_;

    struct RightPanel
    {
        ea::string selectedPath_;

        bool scrollToSelection_{};
    } right_;

    struct CursorForHotkeys
    {
        ea::string selectedPath_{};
    } cursor_;

    struct RenameDialog
    {
        EntryReference entryRef_;
        ea::string popupTitle_;
        ea::string inputBuffer_;
        bool openPending_{};
    } rename_;

    struct DeleteDialog
    {
        EntryReference entryRef_;
        ea::string popupTitle_;
        bool openPending_{};
    } delete_;

    struct CreateDialog
    {
        EntryReference parentEntryRef_;
        ea::string popupTitle_;
        ResourceBrowserFactory* factory_{};
        ea::string inputBuffer_;
        bool openPending_{};
    } create_;
    /// @}

    ea::vector<const FileSystemEntry*> tempEntryList_;
};

}