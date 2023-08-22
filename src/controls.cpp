#include "VCTRenderer.h"

float x_angle = 0.00001f;
float y_angle = 0.00001f;

void Sample::key_pressed(int code)
{
    // Handle forward movement.
    if (code == GLFW_KEY_W)
        m_heading_speed = m_camera_speed;
    else if (code == GLFW_KEY_S)
        m_heading_speed = -m_camera_speed;

    // Handle sideways movement.
    if (code == GLFW_KEY_A)
        m_sideways_speed = -m_camera_speed;
    else if (code == GLFW_KEY_D)
        m_sideways_speed = m_camera_speed;

    // Handle upwards movement.
    if (code == GLFW_KEY_SPACE)
        m_climbing_speed = m_camera_speed;
    else if (code == GLFW_KEY_LEFT_CONTROL)
        m_climbing_speed = -m_camera_speed;

    float sun_angle_delta = 5.0f;

    if (code == GLFW_KEY_UP)
    {
        x_angle += sun_angle_delta;
    }

    if (code == GLFW_KEY_DOWN)
    {
        x_angle -= sun_angle_delta;
    }

    if (code == GLFW_KEY_RIGHT)
    {
        y_angle += sun_angle_delta;
    }

    if (code == GLFW_KEY_LEFT)
    {
        y_angle -= sun_angle_delta;
    }
    

    m_lights.lights[0].direction = glm::angleAxis(glm::radians(y_angle), glm::vec3(0.0f, 0.0f, 1.0f)) * glm::angleAxis(glm::radians(x_angle), glm::vec3(1.0f, 0.0f, 0.0f)) * glm::vec3(0.0f, -1.0f, 0.0f);
    m_shadow_map->set_direction(m_lights.lights[0].direction);
}

void Sample::key_released(int code)
{
    // Handle forward movement.
    if (code == GLFW_KEY_W || code == GLFW_KEY_S)
        m_heading_speed = 0.0f;

    // Handle sideways movement.
    if (code == GLFW_KEY_A || code == GLFW_KEY_D)
        m_sideways_speed = 0.0f;

    // Handle upwards movement.
    if (code == GLFW_KEY_SPACE || code == GLFW_KEY_LEFT_CONTROL)
        m_climbing_speed = 0.0f;
}

void Sample::mouse_pressed(int code)
{
    // Enable mouse look.
    if (code == GLFW_MOUSE_BUTTON_RIGHT)
    {
        m_mouse_look = true;
        glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
}

void Sample::mouse_released(int code)
{
    // Disable mouse look.
    if (code == GLFW_MOUSE_BUTTON_RIGHT)
    {
        m_mouse_look = false;
        glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}