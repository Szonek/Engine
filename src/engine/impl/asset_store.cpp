#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <array>

#include "asset_store.h"
#include "logger.h"

#include <fmt/format.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <SDL_rwops.h>


engine::TextureAssetContext::TextureAssetContext(const std::filesystem::path& file_path)
	: width_(0)
	, height_(0)
	, channels_(0)
	, data_(nullptr)
	, type_(TextureAssetDataType::eCount)
{
	stbi_set_flip_vertically_on_load(true);
	data_ = stbi_load(file_path.string().c_str(), &width_, &height_, &channels_, 0);
	type_ = TextureAssetDataType::eUchar8;
	stbi_set_flip_vertically_on_load(false);
}

engine::TextureAssetContext::TextureAssetContext(TextureAssetContext&& rhs)
	: width_(rhs.width_)
	, height_(rhs.height_)
	, channels_(rhs.channels_)
	, data_(rhs.data_)
	, type_(rhs.type_)
{ 

	rhs.width_ = 0;
	rhs.height_ = 0;
	rhs.channels_ = 0;
	rhs.data_ = nullptr;
	rhs.type_ = TextureAssetDataType::eCount;
}

engine::TextureAssetContext& engine::TextureAssetContext::operator=(TextureAssetContext&& rhs)
{
	if (this != &rhs)
	{
		if (data_)
		{
			stbi_image_free(data_);
		}
		data_ = rhs.data_;
		width_ = rhs.width_;
		height_ = rhs.height_;
		channels_ = rhs.channels_;
		type_ = rhs.type_;

		rhs.data_ = nullptr;
		rhs.width_ = 0;
		rhs.height_ = 0;
		rhs.channels_ = 0;
		rhs.type_ = TextureAssetDataType::eCount;
	}
	return *this;
}

engine::TextureAssetContext::~TextureAssetContext()
{
	if (data_)
	{
		stbi_image_free(data_);
		data_ = nullptr;
	}
}

void engine::AssetStore::configure_base_path(std::string_view path)
{
	base_path_ = path;
}

engine::TextureAssetContext engine::AssetStore::get_texture_data(std::string_view name) const
{
	const std::filesystem::path textures_folder = "textures";
	const auto textures_assets_path = base_path_ / textures_folder;
	const auto full_path = textures_assets_path / name.data();

	return std::move(TextureAssetContext(full_path));
}


engine::FontAssetContext::FontAssetContext(const std::filesystem::path &file_path)
{
	auto* file_handle = SDL_RWFromFile(file_path.string().c_str(), "r");
	if(file_handle)
	{
		std::array<char8_t, 1024> buffer{0};
		while(true)
		{
			const auto bytes_read = SDL_RWread( file_handle, buffer.data(), sizeof(buffer[0]) * buffer.size() );
			if(bytes_read == 0)
			{
				// end of file
				break;
			}
			else if (bytes_read < 0)
			{
				log::log(log::LogLevel::eCritical, fmt::format("Error parsing file: {}. Error msg: {}\n", file_path.string(), SDL_GetError()));
				break;
			}
			data_.insert(data_.end(), buffer.begin(), buffer.begin() + bytes_read);
		}
		//Close file handler
		SDL_RWclose( file_handle );
	}
}

void engine::AssetStore::save_texture(std::string_view name, const void* data, std::uint32_t width, std::uint32_t height, std::uint32_t channels)
{
    const std::filesystem::path textures_folder = "textures";
    const auto textures_assets_path = base_path_ / textures_folder;
    const auto full_path = textures_assets_path / name;
    stbi_write_png(full_path.string().data(), width, height, channels, data, 0);
}

std::string engine::AssetStore::get_shader_source(std::string_view name)
{
	const std::filesystem::path shaders_folder = "shaders";
	const auto shaders_assets_path = base_path_ / shaders_folder;
    const auto file_path = shaders_assets_path / name.data();
    std::string ret = "";
    auto* file_handle = SDL_RWFromFile(file_path.string().c_str(), "r");
    if(file_handle)
    {

        std::array<char8_t, 1024> buffer{0};
        while(true)
        {
            const auto bytes_read = SDL_RWread( file_handle, buffer.data(), sizeof(buffer[0]) * buffer.size() );
            if(bytes_read == 0)
            {
                // end of file
                break;
            }
            else if (bytes_read < 0)
            {
                log::log(log::LogLevel::eCritical, fmt::format("Error parsing file: {}. Error msg: {}\n", file_path.string(), SDL_GetError()));
                break;
            }
            ret += std::string (buffer.data(), buffer.data() + bytes_read);
        }

        //Close file handler
        SDL_RWclose( file_handle );
    }
    return ret;
}

engine::FontAssetContext engine::AssetStore::get_font_data(std::string_view name) const
{
	const std::filesystem::path textures_folder = "fonts";
	const auto textures_assets_path = base_path_ / textures_folder;
	const auto full_path = textures_assets_path / name.data();

	return engine::FontAssetContext(full_path);
}
