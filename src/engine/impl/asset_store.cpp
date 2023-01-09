#include <iostream>
#include <fstream>

#include "asset_store.h"


#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>



engine::TextureAssetContext::TextureAssetContext(const std::filesystem::path& file_path)
	: width_(0)
	, height_(0)
	, channels_(0)
	, data_(nullptr)
	, type_(Type::eCount)
{
	stbi_set_flip_vertically_on_load(true);
	data_ = stbi_load(file_path.string().c_str(), &width_, &height_, &channels_, 0);
	type_ = Type::eUchar8;
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
	rhs.type_ = Type::eCount;
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
		rhs.type_ = Type::eCount;
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

engine::TextureAssetContext engine::AssetStore::get_texture_data(std::string_view name)
{
	const std::filesystem::path textures_folder = "textures";
	const auto textures_assets_path = base_path_ / textures_folder;
	const auto full_path = textures_assets_path / name.data();

	return std::move(TextureAssetContext(full_path));
}

std::string engine::AssetStore::get_shader_source(std::string_view name)
{
	const std::filesystem::path shaders_folder = "shaders";
	const auto shaders_assets_path = base_path_ / shaders_folder;
	std::ifstream file_stream;
	file_stream.exceptions(std::ifstream::failbit | std::ifstream::badbit);
	try
	{
		file_stream.open(shaders_assets_path / name.data());
		std::stringstream string_stream;
		string_stream << file_stream.rdbuf();
		file_stream.close();
		return string_stream.str();
	}
	catch (std::exception e)
	{
		std::cout << std::format("[ERROR][ASSET STORE] Cant read file: {}. Path: {}. Exception: {} \n", name.data(), shaders_assets_path.string(), e.what());
	}
	return "";
}