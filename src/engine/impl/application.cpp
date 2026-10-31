#include "application.h"
#include "graphics.h"
#include "asset_store.h"
#include "scene.h"
#include "logger.h"
#include "gltf_parser.h"
#include "model_prefab.h"
#include "scene_prefab.h"
#include "ui_document.h"
#include "serialization/builtin_types.h"
#include "serialization/game_types.h"
#include "serialization/scene_assets.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <SDL3/SDL.h>

#include <algorithm>
#include <SDL3/SDL_events.h>
#include <fmt/format.h>

#include <map>
#include <span>
#include <iostream>

#include "profiler.h"

namespace
{
struct vertex_attribute_simple_t
{
	std::uint32_t size;
	engine::Geometry::vertex_attribute_t::Type type;

    std::vector<float> range_min;
    std::vector<float> range_max;
};
inline std::vector<engine::Geometry::vertex_attribute_t> create_tightly_packed_vertex_layout(std::span<const vertex_attribute_simple_t> simple_attribs)
{
	using namespace engine;
	std::uint32_t stride = 0;
	std::vector<std::uint32_t> offsets;
	offsets.reserve(simple_attribs.size()); // we will use push_back();

	std::for_each(simple_attribs.begin(), simple_attribs.end(), [&stride, &offsets](const vertex_attribute_simple_t& attrib)
		{
			offsets.push_back(stride);
	std::uint32_t bytes_size = 0;
	switch (attrib.type)
	{
	case Geometry::vertex_attribute_t::Type::eFloat32:
		bytes_size = sizeof(float) * attrib.size;
        break;
    case Geometry::vertex_attribute_t::Type::eUint32:
    case Geometry::vertex_attribute_t::Type::eInt32:
        bytes_size = sizeof(std::uint32_t) * attrib.size;
        break;
    case Geometry::vertex_attribute_t::Type::eUint16:
    case Geometry::vertex_attribute_t::Type::eInt16:
        bytes_size = sizeof(std::uint16_t) * attrib.size;
        break;
    case Geometry::vertex_attribute_t::Type::eUint8:
    case Geometry::vertex_attribute_t::Type::eInt8:
        bytes_size = sizeof(std::uint8_t) * attrib.size;
        break;
    default:
        assert(false && "Unhandled case.");
	}
	stride += bytes_size;
		}
	);

	// calc total stride to the next vertex
	// construct layout
	std::vector<Geometry::vertex_attribute_t> vertex_layout;
	vertex_layout.resize(simple_attribs.size());
	for (std::uint32_t idx = 0; auto & attrib : vertex_layout)
	{
		attrib.index = idx;
		attrib.stride = stride;
		attrib.size = simple_attribs[idx].size;
		attrib.type = simple_attribs[idx].type;
		attrib.offset = offsets[idx];
        attrib.range_min = simple_attribs[idx].range_min;
        attrib.range_max = simple_attribs[idx].range_max;
		idx++;
	}
	return vertex_layout;
}
inline std::vector<engine::Geometry::vertex_attribute_t> create_engine_api_layout(const engine_vertex_attributes_layout_t& verts_layout)
{
    auto to_vert_attr_dt = [](const engine_vertex_attribute_data_type_t& dt)
    {
        switch (dt)
        {
        case ENGINE_VERTEX_ATTRIBUTE_DATA_TYPE_FLOAT32: return engine::Geometry::vertex_attribute_t::Type::eFloat32;

        case ENGINE_VERTEX_ATTRIBUTE_DATA_TYPE_UINT32: return engine::Geometry::vertex_attribute_t::Type::eUint32;
        case ENGINE_VERTEX_ATTRIBUTE_DATA_TYPE_INT32: return engine::Geometry::vertex_attribute_t::Type::eInt32;

        case ENGINE_VERTEX_ATTRIBUTE_DATA_TYPE_UINT16: return engine::Geometry::vertex_attribute_t::Type::eUint16;
        case ENGINE_VERTEX_ATTRIBUTE_DATA_TYPE_INT16: return engine::Geometry::vertex_attribute_t::Type::eInt16;

        case ENGINE_VERTEX_ATTRIBUTE_DATA_TYPE_UINT8: return engine::Geometry::vertex_attribute_t::Type::eUint8;
        case ENGINE_VERTEX_ATTRIBUTE_DATA_TYPE_INT8: return engine::Geometry::vertex_attribute_t::Type::eInt8;
        default:
            return engine::Geometry::vertex_attribute_t::Type::eCount;
        }
    };

    auto to_range_vector = [](const float* data, const auto size)
    {
        std::vector<float> ret(size);
        for (std::size_t i = 0; i < ret.size(); i++)
        {
            ret.at(i) = data[i];
        }
        return ret;
    };

    std::vector<vertex_attribute_simple_t> vertex_layout_simple;
    for (std::size_t i = 0; i < std::size(verts_layout.attributes); i++)
    {
        const auto& attr = verts_layout.attributes[i];
        if (attr.elements_count == 0)
        {
            continue;
        }
        vertex_layout_simple.push_back({ attr.elements_count, to_vert_attr_dt(attr.elements_data_type), to_range_vector(attr.range_min, attr.elements_count), to_range_vector(attr.range_max, attr.elements_count)});
    }
    return create_tightly_packed_vertex_layout(vertex_layout_simple);
}

}  // namespace annoymous

engine::Application::Application(const engine_application_create_desc_t& desc, RenderContext&& rdx, engine_result_code_t& out_code)
    : rdx_(std::move(rdx))
    , default_texture_idx_(ENGINE_INVALID_OBJECT_HANDLE)
    , ui_manager_(rdx_)
    , renderer_(rdx_)
    , play_mode_(desc.enable_editor || desc.play_mode_control, desc.enable_editor)
{
    if (desc.max_delta_time > 0.0f)
    {
        max_delta_time_ = desc.max_delta_time;
    }
    register_builtin_types(types_);
    renderer_.add_feature(std::make_shared<CallbackRenderFeature>("UI", [this](FrameContext& frame)
        {
            add_overlay_pass(frame, "UI", frame.scene_output, [this](rg::PassContext&)
                {
                    ui_manager_.update_state_and_render();
                });
        }), RenderStage::eOverlay);

	{
		const std::vector<std::byte> default_texture_color = { static_cast<std::byte>(255), static_cast<std::byte>(255), static_cast<std::byte>(255) };
		TextureDesc tex2d_desc{};
        tex2d_desc.name = "default_texture";
		tex2d_desc.width = 1;
		tex2d_desc.height = 1;
        tex2d_desc.layout = engine::DataLayout::eRGBA_U8;
        tex2d_desc.data = default_texture_color;
        default_texture_idx_ = add_texture(tex2d_desc);
	}

	timer_.tick();

	out_code = ENGINE_RESULT_CODE_OK;
}

engine::Application::~Application()
{
    if (default_texture_idx_ != ENGINE_INVALID_OBJECT_HANDLE)
    {
        release_texture(default_texture_idx_);
    }
}

engine::Scene* engine::Application::allocate_scene(const engine_scene_create_desc_t& desc)
{
    engine_result_code_t ret_code = ENGINE_RESULT_CODE_FAIL;
    auto ret = new Scene(this, renderer_, desc, ret_code);
    if (ret_code == ENGINE_RESULT_CODE_FAIL)
    {
        delete ret;
        return nullptr;
    }
    on_scene_create(ret);
    return ret;
}

void engine::Application::release_scene(Scene* scene)
{
    if (scene)
    {
        on_scene_release(scene);
        delete scene;
    }
}

engine_result_code_t engine::Application::update_scene(Scene* scene, float delta_time)
{
    ENGINE_PROFILE_SECTION;
    on_scene_update_pre(scene, delta_time);
    // the editor's hooks change the scene for its update (its camera disables the game's) and change it back after
    scene_update_running_ = true;
	const auto ret_code = scene->update(delta_time);
    scene_update_running_ = false;
    on_scene_update_post(scene, delta_time);

    return ret_code;
}

engine_application_frame_begine_info_t engine::Application::begine_frame()
{
    ENGINE_PROFILE_SECTION;
	timer_.tick();

    engine_application_frame_begine_info_t ret{};
    ret.delta_time = std::min(static_cast<float>(timer_.delta_time().count()) / 1000.0f, max_delta_time_);
    ret.events = ENGINE_EVENT_NONE;

    // the data assets whose files changed (the editor watches them), before anything reads them this frame
    if (data_assets_changed_.exchange(false))
    {
        reload_changed_data_assets();
    }

	for(auto& f : finger_info_buffer)
	{
		f.event_type_flags = ENGINE_FINGER_UNKNOWN;
		f.x = -1.0f;
		f.y = -1.0f;
		f.dx = 0.0f;
		f.dy = 0.0f;
	}

    /*
    if (power_save)
        has_event = SDL_WaitEventTimeout(&ev, static_cast<int>(Rml::Math::Min(context->GetNextUpdateDelay(), 10.0) * 1000));
    else
        has_event = SDL_PollEvent(&ev);
    */
    //Handle events on queue
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0)
    {
        on_sdl_event(e);
        ui_manager_.parse_sdl_event(e);

        if (e.type == SDL_EVENT_QUIT)
        {
            request_quit();
        }
        else if (e.type == SDL_EVENT_WINDOW_RESIZED)
        {
            ret.events |= ENGINE_EVENT_WINDOW_RESIZED;
            //log("Window %d resized to %dx%d", e.window.windowID, e.window.data1, e.window.data2);
        }
		else if(e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
		{
			viewport_t vp{};
			vp.width = e.window.data1;
			vp.height = e.window.data2;
			rdx_.set_viewport(vp);
		}
        else if (e.type == SDL_EVENT_WINDOW_MOVED)
        {
            ret.events |= ENGINE_EVENT_WINDOW_MOVED;
        }
		else if(e.type == SDL_EVENT_FINGER_UP)
		{
			//const auto str = fmt::format("[SDL_EVENT_FINGER_UP]: [{}, {}] {}, {}, {}, {}\n", e.tfinger.fingerId, e.tfinger.touchId, e.tfinger.x, e.tfinger.y, e.tfinger.dx, e.tfinger.dy);
			//log::log(log::LogLevel::eTrace, str.c_str());
			//auto& f = finger_info_buffer[e.tfinger.fingerId];
			//f.event_type_flags |= ENGINE_FINGER_UP;
			//f.x = e.tfinger.x;
			//f.y = e.tfinger.y;
		}
		else if(e.type == SDL_EVENT_FINGER_DOWN)
		{
			//const auto str = fmt::format("[SDL_EVENT_FINGER_DOWN]: [{}, {}] {}, {}, {}, {}\n", e.tfinger.fingerId, e.tfinger.touchId, e.tfinger.x, e.tfinger.y, e.tfinger.dx, e.tfinger.dy);
			//log::log(log::LogLevel::eTrace, str.c_str());
			//auto& f = finger_info_buffer[e.tfinger.fingerId];
			//f.event_type_flags |= ENGINE_FINGER_DOWN;
			//f.x = e.tfinger.x;
			//f.y = e.tfinger.y;
		}
		else if(e.type == SDL_EVENT_FINGER_MOTION)
		{
			//auto& f = finger_info_buffer[e.tfinger.fingerId];
			//f.event_type_flags |= ENGINE_FINGER_MOTION;
			//f.x = e.tfinger.x;
			//f.y = e.tfinger.y;
			//f.dx += e.tfinger.dx;
			//f.dy += e.tfinger.dy;
            //
			//const auto str = fmt::format("[SDL_EVENT_FINGER_MOTION]: [{}, {}] {}, {}, {}, {}\n", e.tfinger.fingerId, e.tfinger.touchId, e.tfinger.x, e.tfinger.y, e.tfinger.dx, e.tfinger.dy);
			//log::log(log::LogLevel::eTrace, str.c_str());
		}
        else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
        {
            // ?
        }
        else if(e.type == SDL_EVENT_MOUSE_BUTTON_UP)
        {
            // ?
        }
    }

    // flip the coords so it matches mouse and expected coords are: (0, 0) left bottom corner;  (1,1) top right corner
    for(auto& f : finger_info_buffer)
    {
        f.y = 1.0f - f.y;
        f.dy = -1.0f * f.dy;
    }

    // scene (camera) will submit what to draw, graph is executed in end_frame
    renderer_.begin_frame();

    on_frame_begine(ret);
    // after the editor's Play and Stop (on_frame_begine): a step asked for while paused plays this frame, a frame
    // beginning in Exiting is the play session's last
    play_mode_.begin_frame();
    if (quit_confirmed_)
    {
        ret.events |= ENGINE_EVENT_QUIT;
    }
	return ret;
}

void engine::Application::request_quit()
{
    if (!quit_confirmed_ && on_quit_requested())
    {
        confirm_quit();
    }
}

bool engine::Application::is_play_mode_change_allowed() const
{
    if (scene_update_running_)
    {
        log::log(log::LogLevel::eError, "Play mode: Play and Stop can't be done during a scene update.\n");
        return false;
    }
    return true;
}

engine_result_code_t engine::Application::enter_play_mode(Scene& scene)
{
    if (!is_play_mode_change_allowed())
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    const auto entered = play_mode_.enter([&scene]() { return PlaySnapshot{ scene.save(), scene.get_nav_mesh() != nullptr, scene.get_file_path() }; });
    if (!entered)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    on_play_mode_enter(&scene);
    return ENGINE_RESULT_CODE_OK;
}

engine_result_code_t engine::Application::set_play_mode_paused(bool paused)
{
    return play_mode_.set_paused(paused) ? ENGINE_RESULT_CODE_OK : ENGINE_RESULT_CODE_FAIL;
}

engine_result_code_t engine::Application::step_play_mode()
{
    return play_mode_.step() ? ENGINE_RESULT_CODE_OK : ENGINE_RESULT_CODE_FAIL;
}

engine_result_code_t engine::Application::exit_play_mode()
{
    if (!is_play_mode_change_allowed())
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    return play_mode_.exit() ? ENGINE_RESULT_CODE_OK : ENGINE_RESULT_CODE_FAIL;
}

engine_result_code_t engine::Application::finish_exit_play_mode(Scene& scene)
{
    const auto exited = play_mode_.finish_exit([&scene](const PlaySnapshot& snapshot)
        {
            // the scene is left as it is when the snapshot can't be loaded
            if (!scene.replace(snapshot.scene, "play mode snapshot"))
            {
                return false;
            }
            // the scene the game loaded while playing (a level exit) is the one at Play again
            scene.set_file_path(snapshot.path);
            if (!snapshot.path.empty())
            {
                scene.prefabs().add_scene_file(snapshot.path);
            }
            // derived data, gone with its game object
            if (snapshot.had_nav_mesh)
            {
                scene.build_nav_mesh();
            }
            return true;
        });
    if (!exited)
    {
        return ENGINE_RESULT_CODE_FAIL;
    }
    on_play_mode_exit(&scene);
    return ENGINE_RESULT_CODE_OK;
}

engine_application_frame_end_info_t engine::Application::end_frame()
{
    ENGINE_PROFILE_SECTION;
    on_frame_end();
    // the UI is laid out for the image rendered now; mouse events of the next frame happen while it is on the screen
    ui_manager_.set_viewport(get_scene_viewport());
    renderer_.render_frame();
    rdx_.end_frame();
    ENGINE_PROFILE_FRAME;
	engine_application_frame_end_info_t ret{};
	//ret.success = !glfwWindowShouldClose(rdx_.get_glfw_window());;
    ret.success = true;
	return ret;
}

std::uint32_t engine::Application::add_texture(const TextureDesc& desc)
{
    return textures_atlas_.add_object(desc.name, Texture2D(desc.width, desc.height, true, desc.data.data(), desc.layout, TextureAddressClampMode::eClampToEdge));
}

std::uint32_t engine::Application::add_texture_from_file(std::string_view file_name, std::string_view texture_name, engine_texture_color_space_t /*color_space*/)
{
	return textures_atlas_.add_object(texture_name, Texture2D(file_name, true));
}

std::uint32_t engine::Application::load_texture_file(std::string_view path)
{
    const auto name = std::filesystem::path(path).lexically_normal().generic_string();
    if (const auto loaded = get_texture(name); loaded != ENGINE_INVALID_OBJECT_HANDLE)
    {
        return acquire_texture(loaded) ? loaded : ENGINE_INVALID_OBJECT_HANDLE;
    }
    const auto file_path = AssetStore::get_instance().get_models_base_path() / name;
    const TextureAssetContext image(file_path);
    const std::array layouts = { DataLayout::eR_U8, DataLayout::eCount, DataLayout::eRGB_U8, DataLayout::eRGBA_U8 };
    const auto channels = image.get_channels();
    if (!image.get_data_ptr() || channels < 1 || channels > 4 || layouts[channels - 1] == DataLayout::eCount)
    {
        log::log(log::LogLevel::eError, fmt::format("Failed loading image file {}: it can't be read or has {} channels.\n", file_path.string(), channels));
        return ENGINE_INVALID_OBJECT_HANDLE;
    }
    TextureDesc desc{};
    desc.name = name;
    desc.width = static_cast<std::uint32_t>(image.get_width());
    desc.height = static_cast<std::uint32_t>(image.get_height());
    desc.layout = layouts[channels - 1];
    const auto* data = reinterpret_cast<const std::byte*>(image.get_data_ptr());
    desc.data.assign(data, data + desc.width * desc.height * channels);
    return add_texture(desc);
}

std::uint32_t engine::Application::get_texture(std::string_view name) const
{
    const auto ret = textures_atlas_.get_object(name);
    return ret;
}

const engine::Texture2D* engine::Application::get_texture(std::uint32_t idx) const
{
    return textures_atlas_.get_object(idx);
}

std::string engine::Application::get_texture_name(std::uint32_t idx) const
{
    return textures_atlas_.get_object_name(idx);
}

std::vector<std::pair<std::uint32_t, std::string>> engine::Application::get_texture_names() const
{
    return textures_atlas_.get_object_names();
}

bool engine::Application::acquire_texture(std::uint32_t idx)
{
    return textures_atlas_.acquire(idx);
}

void engine::Application::release_texture(std::uint32_t idx)
{
    textures_atlas_.release(idx);
}

bool engine::Application::add_font_from_file(std::string_view file_name, std::string_view handle_name)
{
    const auto res = ui_manager_.load_font_from_file(file_name, handle_name);
    return res;
}

std::uint32_t engine::Application::add_geometry(const engine_vertex_attributes_layout_t& api_verts_layout, std::int32_t vertex_count, std::span<const std::byte> verts_data, std::span<const uint32_t> inds, std::string_view name)
{
	const auto vertex_layout = create_engine_api_layout(api_verts_layout);
	return geometries_atlas_.add_object(name, Geometry(vertex_layout, verts_data, vertex_count, inds));
}

std::uint32_t engine::Application::get_geometry(std::string_view name) const
{
    return geometries_atlas_.get_object(name);
}

std::string engine::Application::get_geometry_name(std::uint32_t idx) const
{
    return geometries_atlas_.get_object_name(idx);
}

std::vector<std::pair<std::uint32_t, std::string>> engine::Application::get_geometry_names() const
{
    return geometries_atlas_.get_object_names();
}

const engine::Geometry* engine::Application::get_geometry(std::uint32_t idx) const
{
    return geometries_atlas_.get_object(idx);
}

void engine::Application::release_geometry(std::uint32_t idx)
{
    geometries_atlas_.release(idx);
}

std::uint32_t engine::Application::add_skin(const SkinDesc& desc, const ModelNodeDesc& root)
{
    try
    {
        return skins_atlas_.add_object(desc.asset_id, Skin(desc, root));
    }
    catch (const std::exception& e)
    {
        log::log(log::LogLevel::eError, fmt::format("Failed creating skin {}: {}\n", desc.asset_id, e.what()));
        return ENGINE_INVALID_OBJECT_HANDLE;
    }
}

std::uint32_t engine::Application::get_skin(std::string_view name) const
{
    return skins_atlas_.get_object(name);
}

const engine::Skin* engine::Application::get_skin(std::uint32_t idx) const
{
    return skins_atlas_.get_object(idx);
}

std::string engine::Application::get_skin_name(std::uint32_t idx) const
{
    return skins_atlas_.get_object_name(idx);
}

std::vector<std::pair<std::uint32_t, std::string>> engine::Application::get_skin_names() const
{
    return skins_atlas_.get_object_names();
}

void engine::Application::release_skin(std::uint32_t idx)
{
    skins_atlas_.release(idx);
}

std::uint32_t engine::Application::add_animation_set(std::string_view name, std::shared_ptr<const AnimationLibrary> animation_set)
{
    return animation_sets_atlas_.add_object(name, std::move(animation_set));
}

std::uint32_t engine::Application::get_animation_set(std::string_view name) const
{
    return animation_sets_atlas_.get_object(name);
}

std::shared_ptr<const engine::AnimationLibrary> engine::Application::get_animation_set(std::uint32_t idx) const
{
    const auto* animation_set = animation_sets_atlas_.get_object(idx);
    return animation_set ? *animation_set : nullptr;
}

std::string engine::Application::get_animation_set_name(std::uint32_t idx) const
{
    return animation_sets_atlas_.get_object_name(idx);
}

void engine::Application::release_animation_set(std::uint32_t idx)
{
    animation_sets_atlas_.release(idx);
}

std::uint32_t engine::Application::load_prefab(std::string_view path)
{
    // the asset ID: the same file is always named the same way
    const auto name = std::filesystem::path(path).lexically_normal().generic_string();
    // a prefab is named by its file: that file is loaded already
    if (const auto loaded = get_prefab(name); loaded != ENGINE_INVALID_OBJECT_HANDLE)
    {
        return acquire_prefab(loaded) ? loaded : ENGINE_INVALID_OBJECT_HANDLE;
    }
    if (is_scene_prefab_id(name))
    {
        // logs why when it fails
        std::unique_ptr<Prefab> prefab = ScenePrefab::load(name);
        return prefab ? prefabs_atlas_.add_object(name, std::move(prefab)) : ENGINE_INVALID_OBJECT_HANDLE;
    }

    const auto file_path = AssetStore::get_instance().get_models_base_path() / name;
    const auto file_data = AssetStore::get_instance().get_raw_data_content(file_path);
    if (file_data.get_size() == 0)
    {
        log::log(log::LogLevel::eError, fmt::format("Failed loading model file: {}\n", file_path.string()));
        return ENGINE_INVALID_OBJECT_HANDLE;
    }
    auto model = parse_gltf_data_from_memory({ file_data.get_data_ptr(), file_data.get_size() }, file_path.parent_path().string(), name);
    if (model.nodes.empty())
    {
        log::log(log::LogLevel::eError, fmt::format("Model file {} has no nodes, it is not a usable glTF file.\n", file_path.string()));
        return ENGINE_INVALID_OBJECT_HANDLE;
    }
    std::unique_ptr<Prefab> prefab = ModelPrefab::create(*this, std::move(model));
    if (!prefab)
    {
        return ENGINE_INVALID_OBJECT_HANDLE;
    }
    // when it isn't added, the prefab is destroyed here and releases its assets
    return prefabs_atlas_.add_object(name, std::move(prefab));
}

std::uint32_t engine::Application::get_prefab(std::string_view name) const
{
    return prefabs_atlas_.get_object(name);
}

const engine::Prefab* engine::Application::get_prefab(std::uint32_t idx) const
{
    const auto* prefab = prefabs_atlas_.get_object(idx);
    return prefab ? prefab->get() : nullptr;
}

std::string engine::Application::get_prefab_name(std::uint32_t idx) const
{
    return prefabs_atlas_.get_object_name(idx);
}

std::vector<std::pair<std::uint32_t, std::string>> engine::Application::get_prefab_names() const
{
    return prefabs_atlas_.get_object_names();
}

bool engine::Application::acquire_prefab(std::uint32_t idx)
{
    return prefabs_atlas_.acquire(idx);
}

void engine::Application::release_prefab(std::uint32_t idx)
{
    prefabs_atlas_.release(idx);
}

std::uint32_t engine::Application::load_data_asset(std::string_view path)
{
    // the asset ID: the same file is always named the same way
    const auto name = std::filesystem::path(path).lexically_normal().generic_string();
    const auto loaded = data_assets_atlas_.get_object(name);
    // a file being read again references loaded assets too: holding one which references it would keep both alive forever
    if (loading_data_assets_.contains(name) || (loaded != ENGINE_INVALID_OBJECT_HANDLE && references_loading_data_asset(loaded)))
    {
        log::log(log::LogLevel::eError, fmt::format("Data asset {} references itself (directly or through other data assets), the reference is none.\n", name));
        return ENGINE_INVALID_OBJECT_HANDLE;
    }
    if (loaded != ENGINE_INVALID_OBJECT_HANDLE)
    {
        return data_assets_atlas_.acquire(loaded) ? loaded : ENGINE_INVALID_OBJECT_HANDLE;
    }
    loading_data_assets_.insert(name);
    // the assets the value references are held as long as the asset has the value
    auto asset = DataAsset::load(name, AssetStore::get_instance().get_data_base_path() / name, types_, [this] { return std::make_unique<SceneAssets>(*this); },
        [this] { data_assets_changed_ = true; });
    loading_data_assets_.erase(name);
    return asset ? data_assets_atlas_.add_object(name, std::move(asset)) : ENGINE_INVALID_OBJECT_HANDLE;
}

void engine::Application::reload_changed_data_assets()
{
    // a copy: reading a file again can load data assets and release others (the ones only its old value referenced)
    for (const auto& [handle, name] : data_assets_atlas_.get_object_names())
    {
        const auto* asset = data_assets_atlas_.get_object(handle);
        if (!asset)
        {
            continue;
        }
        loading_data_assets_.insert(name);
        (*asset)->reload_if_changed();
        loading_data_assets_.erase(name);
    }
}

bool engine::Application::references_loading_data_asset(std::uint32_t idx) const
{
    const auto* asset = data_assets_atlas_.get_object(idx);
    if (!asset)
    {
        return false;
    }
    if (loading_data_assets_.contains((*asset)->get_name()))
    {
        return true;
    }
    // loaded data assets never reference each other in a cycle, so this ends
    for (const auto referenced : (*asset)->get_data_assets())
    {
        if (references_loading_data_asset(referenced))
        {
            return true;
        }
    }
    return false;
}

void engine::Application::release_data_asset(std::uint32_t idx)
{
    data_assets_atlas_.release(idx);
}

const engine::DataAsset* engine::Application::get_data_asset(std::uint32_t idx) const
{
    const auto* asset = data_assets_atlas_.get_object(idx);
    return asset ? asset->get() : nullptr;
}

std::string engine::Application::get_data_asset_name(std::uint32_t idx) const
{
    return data_assets_atlas_.get_object_name(idx);
}

std::vector<std::pair<std::uint32_t, std::string>> engine::Application::get_data_asset_names() const
{
    return data_assets_atlas_.get_object_names();
}

std::uint32_t engine::Application::add_shader(const std::vector<std::string>& vertex_shader_name, const std::vector<std::string>& fragment_shader_name, std::string_view name)
{
    return shader_atlas_.add_object(name, Shader(vertex_shader_name, fragment_shader_name));
}

std::uint32_t engine::Application::get_shader(std::string_view name) const
{
    return shader_atlas_.get_object(name);
}

std::string engine::Application::get_shader_name(std::uint32_t idx) const
{
    return shader_atlas_.get_object_name(idx);
}

void engine::Application::release_shader(std::uint32_t idx)
{
    shader_atlas_.release(idx);
}

engine::UiDocument engine::Application::load_ui_document(std::string_view file_name)
{
    return ui_manager_.load_document_from_file(file_name);
}

engine::UiDataHandle engine::Application::create_ui_document_data_handle(std::string_view name, std::span<const engine_ui_document_data_binding_t> bindings)
{
    return ui_manager_.create_data_handle(name, bindings);
}

bool engine::Application::keyboard_is_key_down(engine_keyboard_keys_t key)
{
    const auto state = SDL_GetKeyboardState(nullptr);
    return static_cast<bool>(state[key]);
}

engine_fvec2_t engine::Application::mouse_get_coords()
{
	float coord_x = 0.;
	float coord_y = 0.;
    SDL_GetMouseState(&coord_x, &coord_y);

    // relative to the scene, outside [0, 1] when the mouse isn't over it
    const auto viewport = get_scene_viewport();
    const auto x = static_cast<std::int32_t>(std::floor(coord_x)) - static_cast<std::int32_t>(viewport.x);
    const auto y = static_cast<std::int32_t>(std::floor(coord_y)) - static_cast<std::int32_t>(viewport.y);

    engine_fvec2_t ret{};
	ret.x = x / static_cast<float>(viewport.width);
    // flip Y coords so left, bottom corner is (0, 0) and right top is (1, 1)
    ret.y = 1.0f - y / static_cast<float>(viewport.height);
	return ret;
}

engine_fvec2_t engine::Application::mouse_get_game_coords()
{
    if (is_mouse_enabled())
    {
        last_game_mouse_coords_ = mouse_get_coords();
    }
    return last_game_mouse_coords_;
}

engine::viewport_t engine::Application::get_scene_viewport() const
{
    const auto window_size = rdx_.get_window_size_in_pixels();
    return viewport_t{ 0, 0, static_cast<std::uint32_t>(window_size.width), static_cast<std::uint32_t>(window_size.height) };
}

bool engine::Application::mouse_is_button_down(engine_mouse_button_t button)
{
    const auto state = SDL_GetMouseState(nullptr, nullptr);
    return state & SDL_BUTTON_MASK(button);
}

std::array<engine_finger_info_t, 10> engine::Application::get_finger_info_events() const
{
	return finger_info_buffer;
}

const engine::TypeInfo* engine::Application::register_game_type(const engine_type_desc_t& desc, bool component)
{
    return engine::register_game_type(types_, desc, component);
}
