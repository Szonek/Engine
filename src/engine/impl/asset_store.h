#pragma once

#include <string>
#include <filesystem>

namespace engine
{

	class TextureAssetContext
	{
	public:
		enum class Type
		{
			eUchar8 = 0,
			eCount
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
		Type get_type() const { return type_; }

	private:
		std::int32_t width_;
		std::int32_t height_;
		std::int32_t channels_;
		Type type_;
		std::uint8_t* data_;
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
		TextureAssetContext get_texture_data(std::string_view name);
		std::string get_shader_source(std::string_view name);

	protected:
		AssetStore() = default;
		~AssetStore() = default;

	private:
		std::filesystem::path base_path_;
	};


}  // namespace engine