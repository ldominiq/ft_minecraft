#ifndef SKIN_MANAGER_HPP
#define SKIN_MANAGER_HPP

#include <glad/glad.h>
#include <string>
#include <unordered_map>

// Loads PNG skins (one per entity type) as GL_TEXTURE_2D with NEAREST
// filtering and CLAMP_TO_EDGE wrapping
class SkinManager
{
	public:
		SkinManager() = default;
		~SkinManager();

		// Load `path` and register it under `name`. Returns true on success.
		bool load(const std::string& name, const std::string& path);

		// Returns the GL texture handle for `name`, or 0 if not loaded.
		GLuint get(const std::string& name) const;

	private:
		std::unordered_map<std::string, GLuint> skins;
};

#endif
