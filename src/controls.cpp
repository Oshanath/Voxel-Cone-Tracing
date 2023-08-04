#include "VCTRenderer.h"

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