
#ifndef SPACE
#define SPACE

#include <vector>
#include <memory>

#include "Shader.hpp"
#include "cube.hpp"

class Space
{
	protected:

		std::vector<std::shared_ptr<Space>> childs; // maybe put in private

	public:

		glm::mat4 rotation = glm::mat4(1.0f);
		glm::mat4 scale = glm::mat4(1.0f);
		glm::mat4 translation = glm::mat4(1.0f);

		glm::mat4 totalScale = glm::mat4(1.0f);

		virtual ~Space() = default;

		template<typename T>
		void addChild(std::shared_ptr<T> child) {
			static_assert(std::is_base_of_v<Space, T>, "T must derive from Space");
			childs.push_back(child);
		}

		virtual void compute(const glm::mat4 &parentTransform, const glm::mat4& proj, const glm::mat4& view, const Shader &shader);
		virtual void drawScene(const Shader &shader, const glm::mat4& proj, const glm::mat4& view);
};

glm::mat4 extractScaleInverse(const glm::mat4& m);

#endif