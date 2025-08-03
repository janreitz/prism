#include "ast_matcher_view.h"
#include "filesystem_view.h"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#if TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

static void glfw_error_callback(int error, const char *description)
{
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

int main()
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        return 1;
    }

    const char *glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow *window = glfwCreateWindow(
        1280, 720, "Prism - Code Analysis Tool", nullptr, nullptr);
    if (window == nullptr) {
        return 1;
    }

    glfwMakeContextCurrent(window);
    // glfwSwapInterval(0);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // AST analysis is now handled by ASTMatcherView

    // Application state
    bool show_filesystem_view = true;
    bool show_ast_matcher_view = true;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Initialize views
    FilesystemView filesystem_view;
    ASTMatcherView ast_matcher_view;

    while (!glfwWindowShouldClose(window)) {
#if TRACY_ENABLE
        FrameMark;
#endif
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Enable docking
        ImGui::DockSpaceOverViewport(0, nullptr,
                                     ImGuiDockNodeFlags_PassthruCentralNode);

        {
            ImGui::Begin("Prism Control Panel");

            ImGui::Text("Welcome to Prism - Code Analysis Tool");
            ImGui::Separator();

            ImGui::Checkbox("Filesystem View", &show_filesystem_view);
            ImGui::Checkbox("AST Matcher View", &show_ast_matcher_view);

            ImGui::ColorEdit3("clear color",
                              reinterpret_cast<float *>(&clear_color));

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                        1000.0f / io.Framerate, io.Framerate);
            ImGui::End();
        }

        // Render views
        if (show_filesystem_view) {
            show_filesystem_view = filesystem_view.render();
        }

        if (show_ast_matcher_view) {
            show_ast_matcher_view = ast_matcher_view.render();
        }

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w,
                     clear_color.y * clear_color.w,
                     clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}