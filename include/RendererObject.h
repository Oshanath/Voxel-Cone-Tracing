#pragma once

#include <mesh.h>
#include <glm.hpp>
#include <gtc/quaternion.hpp>
#include <gtc/matrix_transform.hpp>

class RenderObject
{
public:
    glm::vec3 position;
    glm::quat rotation;
    float scale;
    dw::Mesh::Ptr mesh;

    inline RenderObject(dw::Mesh::Ptr mesh) :
        position(glm::vec3(0.0f, 0.0f, 0.0f)),
        rotation(glm::angleAxis(0.0f, glm::vec3(0.0f, 1.0f, 0.0f))),
        scale(1.0f),
        mesh(mesh) { }

    inline glm::mat4 get_model()
    {
        auto model = glm::mat4(1.0f);
        model = glm::scale(model, glm::vec3(scale));
        model = glm::mat4_cast(rotation) * model;
        model = glm::translate(model, position);
        return model;
    }

    inline void reset()
    {
        mesh.reset();
    }
};