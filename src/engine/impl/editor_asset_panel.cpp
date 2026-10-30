#include "editor_asset_panel.h"

#include "application.h"
#include "asset_store.h"
#include "animation_controller.h"
#include "serialization/scene_assets.h"

#include "imgui/imgui.h"

#include <fmt/format.h>

#include <algorithm>
#include <system_error>
#include <utility>

namespace
{
// a small drawn icon per kind of file (no icon font), `size` pixels square at the cursor, like the item it stands
// in front of
void draw_icon(engine::AssetFileKind kind, float size)
{
    using engine::AssetFileKind;
    auto* draw_list = ImGui::GetWindowDrawList();
    const auto p = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(size, size));
    const auto at = [p, size](float x, float y) { return ImVec2(p.x + x * size, p.y + y * size); };
    switch (kind)
    {
    case AssetFileKind::eFolder:
        draw_list->AddRectFilled(at(0.05f, 0.15f), at(0.45f, 0.35f), IM_COL32(200, 160, 60, 255), 1.0f);
        draw_list->AddRectFilled(at(0.05f, 0.25f), at(0.95f, 0.85f), IM_COL32(230, 185, 80, 255), 2.0f);
        break;
    case AssetFileKind::eModel:
        // a cube: top, left and right faces
        draw_list->AddQuadFilled(at(0.5f, 0.05f), at(0.92f, 0.28f), at(0.5f, 0.5f), at(0.08f, 0.28f), IM_COL32(140, 190, 255, 255));
        draw_list->AddQuadFilled(at(0.08f, 0.28f), at(0.5f, 0.5f), at(0.5f, 0.95f), at(0.08f, 0.73f), IM_COL32(80, 140, 230, 255));
        draw_list->AddQuadFilled(at(0.5f, 0.5f), at(0.92f, 0.28f), at(0.92f, 0.73f), at(0.5f, 0.95f), IM_COL32(50, 100, 190, 255));
        break;
    case AssetFileKind::eScene:
        // a map: a tile grid
        draw_list->AddRectFilled(at(0.05f, 0.1f), at(0.95f, 0.9f), IM_COL32(80, 170, 100, 255), 2.0f);
        draw_list->AddLine(at(0.5f, 0.1f), at(0.5f, 0.9f), IM_COL32(40, 100, 55, 255));
        draw_list->AddLine(at(0.05f, 0.5f), at(0.95f, 0.5f), IM_COL32(40, 100, 55, 255));
        break;
    case AssetFileKind::eTexture:
        // a picture: a mountain and the sun
        draw_list->AddRectFilled(at(0.05f, 0.1f), at(0.95f, 0.9f), IM_COL32(170, 110, 210, 255), 2.0f);
        draw_list->AddTriangleFilled(at(0.15f, 0.8f), at(0.45f, 0.4f), at(0.75f, 0.8f), IM_COL32(240, 230, 250, 255));
        draw_list->AddCircleFilled(at(0.72f, 0.32f), 0.1f * size, IM_COL32(250, 230, 120, 255));
        break;
    case AssetFileKind::eData:
        // a table: a header row and lines
        draw_list->AddRectFilled(at(0.1f, 0.1f), at(0.9f, 0.9f), IM_COL32(220, 140, 70, 255), 2.0f);
        draw_list->AddRectFilled(at(0.1f, 0.1f), at(0.9f, 0.3f), IM_COL32(170, 95, 40, 255), 2.0f);
        draw_list->AddLine(at(0.2f, 0.5f), at(0.8f, 0.5f), IM_COL32(255, 235, 210, 255));
        draw_list->AddLine(at(0.2f, 0.7f), at(0.8f, 0.7f), IM_COL32(255, 235, 210, 255));
        break;
    case AssetFileKind::eOther:
        // a page with a folded corner
        draw_list->AddRectFilled(at(0.2f, 0.05f), at(0.8f, 0.95f), IM_COL32(150, 150, 150, 255), 1.0f);
        draw_list->AddTriangleFilled(at(0.6f, 0.05f), at(0.8f, 0.05f), at(0.8f, 0.25f), IM_COL32(90, 90, 90, 255));
        break;
    }
}

// the payload type of a file dragged out of the browser, or nullptr when it can't be dragged anywhere
const char* get_payload_type(engine::AssetFileKind kind)
{
    switch (kind)
    {
    case engine::AssetFileKind::eModel:
    case engine::AssetFileKind::eScene: return engine::ASSET_PAYLOAD_PREFAB;
    case engine::AssetFileKind::eTexture: return engine::ASSET_PAYLOAD_TEXTURE;
    case engine::AssetFileKind::eData:
    case engine::AssetFileKind::eFolder:
    case engine::AssetFileKind::eOther: break;
    }
    return nullptr;
}

// makes the last item a drag source of the asset ID; what is dragged shows its icon and name
void asset_drag_source(const char* payload_type, const std::string& asset_id, engine::AssetFileKind kind, std::string_view name)
{
    if (ImGui::BeginDragDropSource())
    {
        ImGui::SetDragDropPayload(payload_type, asset_id.c_str(), asset_id.size() + 1);
        draw_icon(kind, ImGui::GetTextLineHeight());
        ImGui::SameLine();
        ImGui::TextUnformatted(name.data(), name.data() + name.size());
        ImGui::EndDragDropSource();
    }
}

// the part of a model's asset ID after "<model ID>#"
std::string_view model_asset_name(std::string_view asset_id)
{
    const auto separator = asset_id.find('#');
    return separator == std::string_view::npos ? asset_id : asset_id.substr(separator + 1);
}

// the model's meshes or images, each a drag source of its asset ID
void draw_model_asset_rows(const std::vector<std::pair<std::uint32_t, std::string>>& assets, const char* payload_type, engine::AssetFileKind icon,
    const char* tooltip)
{
    for (const auto& [handle, asset_id] : assets)
    {
        ImGui::PushID(asset_id.c_str());
        draw_icon(icon, ImGui::GetTextLineHeight());
        ImGui::SameLine();
        const auto name = model_asset_name(asset_id);
        ImGui::Selectable(std::string(name).c_str());
        ImGui::SetItemTooltip("%s\n%s", asset_id.c_str(), tooltip);
        asset_drag_source(payload_type, asset_id, icon, name);
        ImGui::PopID();
    }
}

constexpr const char* MESH_TOOLTIP = "Drag onto a geometry field of the properties panel";
constexpr const char* IMAGE_TOOLTIP = "Drag onto a texture field of the properties panel";
constexpr const char* PLACE_TOOLTIP = "Drag into the scene view to place it";
}  // namespace

engine::AssetBrowserPanel::AssetBrowserPanel(Application& app)
    : app_(app)
    , assets_root_(AssetStore::get_instance().get_models_base_path().parent_path())
{
}

engine::AssetBrowserPanel::~AssetBrowserPanel()
{
    clear_selection();
    for (auto& [path, model] : expanded_models_)
    {
        close_model(model);
    }
}

engine::AssetBrowserPanel::model_contents_t engine::AssetBrowserPanel::get_model_contents(const std::string& model_id) const
{
    model_contents_t contents;
    contents.meshes = get_model_assets(app_.get_geometry_names(), model_id);
    contents.images = get_model_assets(app_.get_texture_names(), model_id);
    if (const auto skins = get_model_assets(app_.get_skin_names(), model_id); !skins.empty())
    {
        contents.skin = model_asset_name(skins.front().second);
    }
    const auto animation_set = app_.get_animation_set(model_id + "#animations");
    if (const auto animations = animation_set == ENGINE_INVALID_OBJECT_HANDLE ? nullptr : app_.get_animation_set(animation_set))
    {
        for (const auto& [name, clip] : animations->animations())
        {
            contents.animations.push_back(name);
        }
        std::ranges::sort(contents.animations);
    }
    return contents;
}

engine::AssetBrowserPanel::opened_model_t engine::AssetBrowserPanel::open_model(const std::string& model_id)
{
    opened_model_t model;
    model.prefab = app_.load_prefab(model_id);
    if (model.prefab != ENGINE_INVALID_OBJECT_HANDLE)
    {
        model.contents = get_model_contents(model_id);
    }
    return model;
}

void engine::AssetBrowserPanel::close_model(opened_model_t& model)
{
    if (model.prefab != ENGINE_INVALID_OBJECT_HANDLE)
    {
        app_.release_prefab(std::exchange(model.prefab, ENGINE_INVALID_OBJECT_HANDLE));
    }
    model.contents = {};
}

void engine::AssetBrowserPanel::clear_selection()
{
    close_model(inspected_model_);
    if (inspected_texture_ != ENGINE_INVALID_OBJECT_HANDLE)
    {
        app_.release_texture(std::exchange(inspected_texture_, ENGINE_INVALID_OBJECT_HANDLE));
    }
    if (inspected_data_asset_ != ENGINE_INVALID_OBJECT_HANDLE)
    {
        app_.release_data_asset(std::exchange(inspected_data_asset_, ENGINE_INVALID_OBJECT_HANDLE));
    }
    selected_.reset();
}

void engine::AssetBrowserPanel::select(const asset_entry_t& entry)
{
    clear_selection();
    selected_ = entry;
    // loaded to show what it holds; logs why when it can't
    if (const auto asset_id = get_asset_id(entry))
    {
        if (entry.kind == AssetFileKind::eModel)
        {
            inspected_model_ = open_model(*asset_id);
        }
        else if (entry.kind == AssetFileKind::eTexture)
        {
            inspected_texture_ = app_.load_texture_file(*asset_id);
        }
        else if (entry.kind == AssetFileKind::eData)
        {
            inspected_data_asset_ = app_.load_data_asset(*asset_id);
        }
    }
}

std::string engine::AssetBrowserPanel::get_selected_asset() const
{
    return selected_ ? selected_->path : std::string();
}

void engine::AssetBrowserPanel::set_selected_asset(const std::string& path)
{
    if (path.empty())
    {
        clear_selection();
        return;
    }
    // only files are selected (a folder opens instead)
    const auto name = std::filesystem::path(path).filename().generic_string();
    select({ name, path, get_asset_file_kind(path, false) });
}

void engine::AssetBrowserPanel::refresh()
{
    folders_.clear();
    const auto exists = [this](const std::string& path)
        {
            std::error_code error;
            return std::filesystem::exists(assets_root_ / path, error);
        };
    // an opened model whose file is gone is never drawn again, so nothing else would release it
    for (auto it = expanded_models_.begin(); it != expanded_models_.end();)
    {
        if (exists(it->first))
        {
            ++it;
            continue;
        }
        close_model(it->second);
        it = expanded_models_.erase(it);
    }
    if (selected_ && !exists(selected_->path))
    {
        clear_selection();
    }
    if (!current_folder_.empty() && !exists(current_folder_))
    {
        current_folder_.clear();
    }
}

const std::vector<engine::asset_entry_t>& engine::AssetBrowserPanel::get_folder(const std::string& folder)
{
    auto it = folders_.find(folder);
    if (it == folders_.end())
    {
        it = folders_.emplace(folder, list_asset_folder(assets_root_, folder)).first;
    }
    return it->second;
}

std::optional<std::string> engine::AssetBrowserPanel::draw()
{
    if (ImGui::Button("Refresh"))
    {
        refresh();
    }
    ImGui::SetItemTooltip("List the folders again (files added, removed or renamed outside the editor)");
    ImGui::SameLine();
    draw_path_bar();

    std::optional<std::string> clicked;
    if (ImGui::BeginTable("##asset_browser", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableSetupColumn("Folders", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("Files", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextColumn();
        if (ImGui::BeginChild("##folders"))
        {
            draw_folder_tree("", "assets");
            reveal_current_folder_ = false;
        }
        ImGui::EndChild();
        ImGui::TableNextColumn();
        if (ImGui::BeginChild("##files"))
        {
            clicked = draw_files();
        }
        ImGui::EndChild();
        ImGui::EndTable();
    }
    return clicked;
}

void engine::AssetBrowserPanel::draw_path_bar()
{
    // the open folder's path, each part opens its folder
    std::string target;
    bool clicked = ImGui::SmallButton("assets");
    std::string path;
    std::size_t start = 0;
    while (start < current_folder_.size())
    {
        const auto end = std::min(current_folder_.find('/', start), current_folder_.size());
        path = current_folder_.substr(0, end);
        ImGui::SameLine(0.0f, 2.0f);
        ImGui::TextUnformatted("/");
        ImGui::SameLine(0.0f, 2.0f);
        ImGui::PushID(path.c_str());
        if (ImGui::SmallButton(current_folder_.substr(start, end - start).c_str()))
        {
            target = path;
            clicked = true;
        }
        ImGui::PopID();
        start = end + 1;
    }
    if (clicked)
    {
        current_folder_ = target;
        reveal_current_folder_ = true;
    }
}

void engine::AssetBrowserPanel::draw_folder_tree(const std::string& folder, std::string_view name)
{
    const auto& entries = get_folder(folder);
    const bool has_subfolders = std::ranges::any_of(entries, [](const asset_entry_t& e) { return e.kind == AssetFileKind::eFolder; });
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
    flags |= folder == current_folder_ ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None;
    flags |= has_subfolders ? ImGuiTreeNodeFlags_None : ImGuiTreeNodeFlags_Leaf;
    flags |= folder.empty() ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None;
    if (reveal_current_folder_ && (folder.empty() || current_folder_.starts_with(folder + "/")))
    {
        ImGui::SetNextItemOpen(true);
    }
    const bool open = ImGui::TreeNodeEx((std::string(name) + "##" + folder).c_str(), flags);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
    {
        current_folder_ = folder;
    }
    if (open)
    {
        for (const auto& entry : entries)
        {
            if (entry.kind == AssetFileKind::eFolder)
            {
                draw_folder_tree(entry.path, entry.name);
            }
        }
        ImGui::TreePop();
    }
}

std::optional<std::string> engine::AssetBrowserPanel::draw_files()
{
    std::optional<std::string> open_folder;
    std::optional<std::string> clicked;
    const auto icon_size = ImGui::GetTextLineHeight();
    for (const auto& entry : get_folder(current_folder_))
    {
        ImGui::PushID(entry.path.c_str());
        const auto asset_id = get_asset_id(entry);
        // a model opens like a folder, listing its meshes and images to drag onto fields (like Unity's sub-assets)
        auto expanded = expanded_models_.find(entry.path);
        if (entry.kind == AssetFileKind::eModel && asset_id)
        {
            // a text-high arrow without a frame, like a tree node's
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 0, 0, 0));
            const bool toggled = ImGui::ArrowButton("##expand", expanded == expanded_models_.end() ? ImGuiDir_Right : ImGuiDir_Down);
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
            if (toggled)
            {
                if (expanded == expanded_models_.end())
                {
                    expanded = expanded_models_.emplace(entry.path, open_model(*asset_id)).first;
                }
                else
                {
                    close_model(expanded->second);
                    expanded_models_.erase(expanded);
                    expanded = expanded_models_.end();
                }
            }
            ImGui::SetItemTooltip("Show the model's meshes and images");
        }
        else
        {
            ImGui::Dummy(ImVec2(icon_size, icon_size));
        }
        ImGui::SameLine();
        draw_icon(entry.kind, icon_size);
        ImGui::SameLine();
        const bool is_selected = selected_ && selected_->path == entry.path;
        if (ImGui::Selectable(entry.name.c_str(), is_selected, ImGuiSelectableFlags_AllowDoubleClick))
        {
            if (entry.kind == AssetFileKind::eFolder)
            {
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    open_folder = entry.path;
                }
            }
            else if (!is_selected)
            {
                clicked = entry.path;
            }
        }
        const auto* payload_type = get_payload_type(entry.kind);
        if (asset_id && payload_type)
        {
            const auto* hint = entry.kind == AssetFileKind::eTexture ? IMAGE_TOOLTIP : PLACE_TOOLTIP;
            ImGui::SetItemTooltip("%s\n%s", asset_id->c_str(), hint);
            asset_drag_source(payload_type, *asset_id, entry.kind, entry.name);
        }
        if (expanded != expanded_models_.end() && expanded->second.prefab != ENGINE_INVALID_OBJECT_HANDLE)
        {
            ImGui::Indent(2.0f * icon_size);
            draw_model_asset_rows(expanded->second.contents.meshes, ASSET_PAYLOAD_GEOMETRY, AssetFileKind::eModel, MESH_TOOLTIP);
            draw_model_asset_rows(expanded->second.contents.images, ASSET_PAYLOAD_TEXTURE, AssetFileKind::eTexture, IMAGE_TOOLTIP);
            ImGui::Unindent(2.0f * icon_size);
        }
        ImGui::PopID();
    }
    if (open_folder)
    {
        current_folder_ = *open_folder;
        reveal_current_folder_ = true;
    }
    return clicked;
}

void engine::AssetBrowserPanel::draw_inspector()
{
    if (!selected_)
    {
        return;
    }
    draw_icon(selected_->kind, ImGui::GetTextLineHeight());
    ImGui::SameLine();
    ImGui::TextUnformatted(selected_->name.c_str());
    ImGui::TextWrapped("Path: assets/%s", selected_->path.c_str());
    std::error_code error;
    if (const auto size = std::filesystem::file_size(assets_root_ / selected_->path, error); !error)
    {
        ImGui::Text("Size: %.1f KB", static_cast<double>(size) / 1024.0);
    }
    const auto asset_id = get_asset_id(*selected_);
    if (asset_id)
    {
        ImGui::TextWrapped("Asset ID: %s", asset_id->c_str());
    }
    switch (selected_->kind)
    {
    case AssetFileKind::eModel:
        if (asset_id)
        {
            draw_model_details();
        }
        else
        {
            ImGui::TextWrapped("Only models under models/ can be loaded: they are named by their path there.");
        }
        break;
    case AssetFileKind::eScene:
        ImGui::TextWrapped("Drag it into the scene view to place it as a scene prefab.");
        break;
    case AssetFileKind::eTexture:
        if (asset_id)
        {
            draw_texture_details();
        }
        else
        {
            ImGui::TextWrapped("Only images under models/ can be used as assets: they are named by their path there.");
        }
        break;
    case AssetFileKind::eData:
        draw_data_asset_details();
        break;
    case AssetFileKind::eFolder:
    case AssetFileKind::eOther:
        ImGui::TextWrapped("Not an asset the editor can use.");
        break;
    }
}

void engine::AssetBrowserPanel::draw_data_asset_details()
{
    const auto* asset = inspected_data_asset_ == ENGINE_INVALID_OBJECT_HANDLE ? nullptr : app_.get_data_asset(inspected_data_asset_);
    if (!asset)
    {
        ImGui::TextWrapped("The data asset couldn't be loaded (see the log).");
        return;
    }
    ImGui::Text("Type: %s", asset->get_type().name.c_str());
    ImGui::TextWrapped("Edit its file: the change shows here and in the game at once.");
    // its value as the file has it, read-only (editing it here comes with saving from the editor)
    const SceneAssets assets(app_);
    ImGui::SeparatorText("Value");
    ImGui::TextUnformatted(save_data_asset(asset->get_type(), asset->get_value(), assets).c_str());
}

void engine::AssetBrowserPanel::draw_model_details()
{
    if (inspected_model_.prefab == ENGINE_INVALID_OBJECT_HANDLE)
    {
        ImGui::TextWrapped("The model couldn't be loaded (see the log).");
        return;
    }
    const auto& contents = inspected_model_.contents;
    ImGui::TextWrapped("Drag it into the scene view to place it.");
    ImGui::SeparatorText(fmt::format("Meshes ({})", contents.meshes.size()).c_str());
    draw_model_asset_rows(contents.meshes, ASSET_PAYLOAD_GEOMETRY, AssetFileKind::eModel, MESH_TOOLTIP);
    ImGui::SeparatorText(fmt::format("Images ({})", contents.images.size()).c_str());
    draw_model_asset_rows(contents.images, ASSET_PAYLOAD_TEXTURE, AssetFileKind::eTexture, IMAGE_TOOLTIP);

    ImGui::SeparatorText("Skin");
    ImGui::TextUnformatted(contents.skin.empty() ? "None" : contents.skin.c_str());

    ImGui::SeparatorText(fmt::format("Animations ({})", contents.animations.size()).c_str());
    for (const auto& clip : contents.animations)
    {
        ImGui::BulletText("%s", clip.c_str());
    }
}

void engine::AssetBrowserPanel::draw_texture_details()
{
    const auto* texture = inspected_texture_ == ENGINE_INVALID_OBJECT_HANDLE ? nullptr : app_.get_texture(inspected_texture_);
    if (!texture)
    {
        ImGui::TextWrapped("The image couldn't be loaded (see the log).");
        return;
    }
    const auto width = static_cast<float>(texture->get_width());
    const auto height = static_cast<float>(texture->get_height());
    ImGui::Text("%u x %u", texture->get_width(), texture->get_height());
    ImGui::TextWrapped("Drag it onto a texture field of the properties panel.");
    // the whole image, at most the panel's width or 256 pixels
    const float scale = std::min({ 1.0f, ImGui::GetContentRegionAvail().x / std::max(width, 1.0f), 256.0f / std::max(width, 1.0f) });
    ImGui::Image(static_cast<ImTextureID>(texture->get_imgui_texture_id()), ImVec2(width * scale, height * scale));
    asset_drag_source(ASSET_PAYLOAD_TEXTURE, *get_asset_id(*selected_), selected_->kind, selected_->name);
}
