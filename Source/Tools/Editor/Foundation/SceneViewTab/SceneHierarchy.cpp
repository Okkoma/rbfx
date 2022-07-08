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

#include "../../Core/IniHelpers.h"
#include "../../Foundation/SceneViewTab/SceneHierarchy.h"

#include <Urho3D/Scene/Component.h>

#include <IconFontCppHeaders/IconsFontAwesome6.h>

namespace Urho3D
{

namespace
{

ea::string GetNodeTitle(Node* node)
{
    const bool isScene = node->GetParent() == nullptr;
    ea::string title = isScene ? ICON_FA_CUBES " " : ICON_FA_CUBE " ";
    if (!node->GetName().empty())
        title += node->GetName();
    else if (isScene)
        title += "Scene";
    else
        title += Format("Node {}", node->GetID());
    return title;
}

}

void Foundation_SceneHierarchy(Context* context, SceneViewTab* sceneViewTab)
{
    sceneViewTab->RegisterAddon<SceneHierarchy>();
}

SceneHierarchy::SceneHierarchy(SceneViewTab* sceneViewTab)
    : SceneViewAddon(sceneViewTab)
{
}

void SceneHierarchy::WriteIniSettings(ImGuiTextBuffer& output)
{
    WriteIntToIni(output, "SceneHierarchy.ShowComponents", showComponents_);
    WriteIntToIni(output, "SceneHierarchy.ShowTemporary", showTemporary_);
}

void SceneHierarchy::ReadIniSettings(const char* line)
{
    if (const auto value = ReadIntFromIni(line, "SceneHierarchy.ShowComponents"))
        showComponents_ = *value != 0;
    if (const auto value = ReadIntFromIni(line, "SceneHierarchy.ShowTemporary"))
        showTemporary_ = *value != 0;
}

void SceneHierarchy::RenderContent()
{
    SceneViewPage* activePage = owner_->GetActivePage();
    if (!activePage)
        return;

    RenderToolbar(*activePage);

    BeginRangeSelection();

    const ImGuiStyle& style = ui::GetStyle();
    ui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x, 0));
    if (search_.lastQuery_.empty())
    {
        RenderNode(*activePage, activePage->scene_);
    }
    else
    {
        for (Node* node : search_.lastResults_)
        {
            if (node && (showTemporary_ || !node->IsTemporaryEffective()))
                RenderNode(*activePage, node);
        }
    }
    ui::PopStyleVar();

    EndRangeSelection(*activePage);
}

void SceneHierarchy::RenderContextMenuItems()
{

}

void SceneHierarchy::RenderMenu()
{
    if (owner_ && !reentrant_)
    {
        reentrant_ = true;
        owner_->RenderMenu();
        reentrant_ = false;
    }
}

void SceneHierarchy::ApplyHotkeys(HotkeyManager* hotkeyManager)
{
    if (owner_ && !reentrant_)
    {
        reentrant_ = true;
        owner_->ApplyHotkeys(hotkeyManager);
        reentrant_ = false;
    }
}

void SceneHierarchy::RenderToolbar(SceneViewPage& page)
{
    if (Widgets::ToolbarButton(ICON_FA_CLOCK, "Show Temporary Nodes & Components", showTemporary_))
        showTemporary_ = !showTemporary_;
    if (Widgets::ToolbarButton(ICON_FA_DIAGRAM_PROJECT, "Show Components", showComponents_))
        showComponents_ = !showComponents_;

    ui::BeginDisabled();
    Widgets::ToolbarButton(ICON_FA_MAGNIFYING_GLASS);
    ui::EndDisabled();

    const bool sceneChanged = search_.lastScene_ != page.scene_;
    const bool queryChanged = ui::InputText("##Rename", &search_.currentQuery_);
    if (queryChanged || sceneChanged)
        UpdateSearchResults(page);
}

void SceneHierarchy::RenderNode(SceneViewPage& page, Node* node)
{
    if (!showTemporary_ && node->IsTemporary())
        return;

    UpdateActiveObjectVisibility(page, node);

    const bool isEmpty = node->GetChildren().empty() && (!showComponents_ || node->GetComponents().empty());
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
        | ImGuiTreeNodeFlags_OpenOnDoubleClick
        | ImGuiTreeNodeFlags_SpanAvailWidth
        | ImGuiTreeNodeFlags_AllowItemOverlap;
    if (node->GetParent() == nullptr)
        flags |= ImGuiTreeNodeFlags_DefaultOpen;
    if (page.selection_.IsSelected(node))
        flags |= ImGuiTreeNodeFlags_Selected;
    if (isEmpty)
        flags |= ImGuiTreeNodeFlags_Leaf;

    ui::PushID(static_cast<void*>(node));
    const bool opened = ui::TreeNodeEx(GetNodeTitle(node).c_str(), flags);
    ProcessRangeSelection(node, opened);

    if ((ui::IsItemClicked(MOUSEB_LEFT) || ui::IsItemClicked(MOUSEB_RIGHT)) && !ui::IsItemToggledOpen())
    {
        const bool toggleSelect = ui::IsKeyDown(KEY_CTRL);
        const bool rangeSelect = ui::IsKeyDown(KEY_SHIFT);
        ProcessObjectSelected(page, node, toggleSelect, rangeSelect);
    }

    if (opened)
    {
        if (showComponents_)
        {
            for (Component* component : node->GetComponents())
                RenderComponent(page, component);
        }

        for (Node* child : node->GetChildren())
            RenderNode(page, child);

        ui::TreePop();
    }
    ui::PopID();
}

void SceneHierarchy::RenderComponent(SceneViewPage& page, Component* component)
{
    if (component->IsTemporary() && !showTemporary_)
        return;

    UpdateActiveObjectVisibility(page, component);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
        | ImGuiTreeNodeFlags_OpenOnDoubleClick
        | ImGuiTreeNodeFlags_SpanAvailWidth
        | ImGuiTreeNodeFlags_AllowItemOverlap
        | ImGuiTreeNodeFlags_Leaf;
    if (page.selection_.IsSelected(component))
        flags |= ImGuiTreeNodeFlags_Selected;

    ui::PushID(static_cast<void*>(component));
    const bool opened = ui::TreeNodeEx(component->GetTypeName().c_str(), flags);
    ProcessRangeSelection(component, opened);

    if (ui::IsItemClicked(MOUSEB_LEFT) || ui::IsItemClicked(MOUSEB_RIGHT))
    {
        const bool toggleSelect = ui::IsKeyDown(KEY_CTRL);
        const bool rangeSelect = ui::IsKeyDown(KEY_SHIFT);
        ProcessObjectSelected(page, component, toggleSelect, rangeSelect);
    }

    if (opened)
        ui::TreePop();
    ui::PopID();
}

void SceneHierarchy::ProcessObjectSelected(SceneViewPage& page, Object* object, bool toggle, bool range)
{
    SceneSelection& selection = page.selection_;
    Object* activeObject = selection.GetActiveObject();

    if (toggle)
        selection.SetSelected(object, !selection.IsSelected(object));
    else if (range && activeObject && wasActiveObjectVisible_ && activeObject != object)
        rangeSelection_.pendingRequest_ = RangeSelectionRequest{WeakPtr<Object>(activeObject), WeakPtr<Object>(object)};
    else
    {
        selection.Clear();
        selection.SetSelected(object, true);
    }
}

void SceneHierarchy::UpdateActiveObjectVisibility(SceneViewPage& page, Object* currentItem)
{
    if (page.selection_.GetActiveObject() == currentItem)
        isActiveObjectVisible_ = true;
}

void SceneHierarchy::BeginRangeSelection()
{
    wasActiveObjectVisible_ = isActiveObjectVisible_;
    isActiveObjectVisible_ = false;
    rangeSelection_.result_.clear();
    rangeSelection_.isActive_ = false;
    rangeSelection_.currentRequest_ = rangeSelection_.pendingRequest_;
    rangeSelection_.pendingRequest_ = ea::nullopt;
}

void SceneHierarchy::ProcessRangeSelection(Object* currentObject, bool open)
{
    if (!rangeSelection_.currentRequest_)
        return;

    const WeakPtr<Object> weakObject{currentObject};
    const bool isBorder = rangeSelection_.currentRequest_->IsBorder(currentObject);

    if (!rangeSelection_.isActive_ && isBorder)
    {
        rangeSelection_.isActive_ = true;
        rangeSelection_.result_.push_back(weakObject);
    }
    else if (rangeSelection_.isActive_ && isBorder)
    {
        rangeSelection_.result_.push_back(weakObject);
        rangeSelection_.isActive_ = false;
        rangeSelection_.currentRequest_ = ea::nullopt;
    }
    else if (rangeSelection_.isActive_)
    {
        rangeSelection_.result_.push_back(weakObject);
    }
}

void SceneHierarchy::EndRangeSelection(SceneViewPage& page)
{
    rangeSelection_.currentRequest_ = ea::nullopt;

    if (!rangeSelection_.isActive_)
    {
        for (Object* object : rangeSelection_.result_)
            page.selection_.SetSelected(object, true);
    }
}

void SceneHierarchy::UpdateSearchResults(SceneViewPage& page)
{
    const bool sceneChanged = search_.lastScene_ != page.scene_;
    search_.lastScene_ = page.scene_;

    // Early return if search was canceled
    if (search_.currentQuery_.empty())
    {
        search_.lastResults_.clear();
        search_.lastQuery_.clear();
        return;
    }

    const bool resultsExpired = sceneChanged
        || search_.lastResults_.empty()
        || !search_.lastQuery_.contains(search_.currentQuery_);
    search_.lastQuery_ = search_.currentQuery_;

    if (resultsExpired)
    {
        ea::vector<Node*> children;
        page.scene_->GetChildren(children, true);

        search_.lastResults_.clear();
        for (Node* child : children)
        {
            if (child->GetName().contains(search_.currentQuery_, false))
                search_.lastResults_.emplace_back(child);
        }
    }
    else
    {
        ea::erase_if(search_.lastResults_, [&](const WeakPtr<Node>& node)
        {
            return !node || !node->GetName().contains(search_.currentQuery_, false);
        });
    }
}

}