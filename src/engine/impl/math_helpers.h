#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_access.hpp>

namespace engine
{
inline glm::mat4 compute_model_matrix(const glm::vec3& glm_pos, const glm::vec3& glm_rot)
{
    auto model_identity = glm::mat4{ 1.0f };
    auto translation = glm::translate(model_identity, glm_pos);
    translation *= glm::toMat4(glm::quat(glm_rot));
    return translation;
}
inline glm::mat4 compute_model_matrix(const glm::vec3& glm_pos, const glm::vec3& glm_rot, const glm::vec3& glm_scl)
{
    auto model_identity = glm::mat4{ 1.0f };
    auto translation = glm::translate(model_identity, glm_pos);
    translation *= glm::toMat4(glm::quat(glm_rot));
    //translation = glm::rotate(translation, glm::radians(0.0f), glm::vec3(1.0f, 0.3f, 0.5f));
    translation = glm::scale(translation, glm_scl);
    return translation;
}
} // namespace engine