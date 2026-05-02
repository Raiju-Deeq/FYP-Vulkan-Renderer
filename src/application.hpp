/**
 * @file application.hpp
 * @brief Top-level application lifecycle coordinator.
 */

#ifndef FYP_VULKAN_RENDERER_APPLICATION_HPP
#define FYP_VULKAN_RENDERER_APPLICATION_HPP

/**
 * @class Application
 * @brief Owns the high-level init, render loop and teardown sequence.
 */
class Application
{
public:
    /**
     * @brief Runs the renderer until the window closes or initialisation fails.
     * @return EXIT_SUCCESS on a clean shutdown, EXIT_FAILURE on setup failure.
     */
    int run();
};

#endif // FYP_VULKAN_RENDERER_APPLICATION_HPP
