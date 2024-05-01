#pragma once

#include <string>
#include <filesystem>
#include <vector>

namespace engine
{
class TextureAssetContext
{
public:
    enum class TextureAssetDataType
    {
        eUchar8 = 0,
        eCount = 1
    };
public:
	TextureAssetContext(const std::filesystem::path& file_path);

	TextureAssetContext(const TextureAssetContext&) = delete;
	TextureAssetContext(TextureAssetContext&& rhs);
	TextureAssetContext& operator=(const TextureAssetContext&) = delete;
	TextureAssetContext& operator=(TextureAssetContext&& rhs);

	~TextureAssetContext();

	std::int32_t get_width() const { return width_; }
	std::int32_t get_height() const { return height_; }
	std::int32_t get_channels() const { return channels_; }
	const std::uint8_t* get_data_ptr() const { return data_; }
    TextureAssetDataType get_type() const { return type_; }

private:
	std::int32_t width_;
	std::int32_t height_;
	std::int32_t channels_;
    TextureAssetDataType type_;
	std::uint8_t* data_;
};

class RawDataFileContext
{
public:
	RawDataFileContext(const std::filesystem::path& file_path);

	RawDataFileContext(const RawDataFileContext&) = delete;
	RawDataFileContext(RawDataFileContext&& rhs) = default;
	RawDataFileContext& operator=(const RawDataFileContext&) = delete;
	RawDataFileContext& operator=(RawDataFileContext&& rhs) = default;

	~RawDataFileContext() = default;

	std::size_t get_size() const { return data_.size(); }
	const std::uint8_t* get_data_ptr() const { return data_.data(); }
    const std::vector<std::uint8_t>& get_vector() const {return data_;}

private:
	std::vector<std::uint8_t> data_;
};

class AssetStore
{
public:
	static AssetStore& get_instance()
	{
		static AssetStore instance;
		return instance;
	}

	// delete copy and move constructors and assign operators
	AssetStore(const AssetStore&) = delete;
	AssetStore(AssetStore&&) = delete;
	AssetStore& operator=(const AssetStore&) = delete;
	AssetStore& operator=(AssetStore&&) = delete;

	void configure_base_path(std::string_view path);
	RawDataFileContext get_font_data(std::string_view name) const;
    std::filesystem::path get_font_base_path() const;
    std::filesystem::path get_ui_docs_base_path() const;
    std::filesystem::path get_textures_base_path() const;
	TextureAssetContext get_texture_data(std::string_view name) const;
	RawDataFileContext get_model_data(std::string_view name) const;
    void save_texture(std::string_view name, const void* data, std::uint32_t width, std::uint32_t height, std::uint32_t channels);
	std::string get_shader_source(std::string_view name);

protected:
	AssetStore() = default;
	~AssetStore() = default;

private:
	std::filesystem::path base_path_;
};

}  // namespace engine