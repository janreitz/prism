#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include "treemap_widget.h"
#include "example_tree_node.h"

static void glfw_error_callback(int error, const char *description)
{
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

int main()
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
    {
        return 1;
    }

    const char *glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow *window = glfwCreateWindow(1280, 720, "Prism - Code Analysis Tool", nullptr, nullptr);
    if (window == nullptr)
    {
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    bool show_demo_window = true;
    bool show_another_window = false;
    bool show_treemap_window = true;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Create sample tree
    auto tree_root = ExampleTreeNode::create_sample_tree();
    TreeMapWidget<ExampleTreeNode> treemap(*tree_root);
    
    // Set up coloring strategy
    treemap.set_coloring_strategy([](const ExampleTreeNode& node) -> ImU32 {
        std::string name = node.name();
        if (name.ends_with(".cpp")) return IM_COL32(70, 130, 180, 255);   // Blue for C++ files
        if (name.ends_with(".h")) return IM_COL32(255, 140, 0, 255);      // Orange for headers
        if (name.ends_with(".md")) return IM_COL32(60, 179, 113, 255);    // Green for docs
        if (name.ends_with("/")) return IM_COL32(186, 85, 211, 255);      // Purple for directories
        return IM_COL32(220, 220, 220, 255);                              // Gray for others
    });

    std::string hovered_info = "Hover over a section to see details";
    std::string selected_info = "Click on a section to select it";

    treemap.add_on_node_hover([&](const ExampleTreeNode &node) {
        hovered_info = "Hovered: " + node.name() + " (Size: " + std::to_string(static_cast<int>(node.size())) + ")";
    });

    treemap.add_on_node_click([&](const ExampleTreeNode &node) {
        selected_info = "Selected: " + node.name() + " (Size: " + std::to_string(static_cast<int>(node.size())) + ")";
    });

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (show_demo_window)
        {
            ImGui::ShowDemoWindow(&show_demo_window);
        }

        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Prism Control Panel");

            ImGui::Text("Welcome to Prism - Code Analysis Tool");
            ImGui::Separator();

            ImGui::Checkbox("Demo Window", &show_demo_window);
            ImGui::Checkbox("Another Window", &show_another_window);
            ImGui::Checkbox("TreeMap Window", &show_treemap_window);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
            ImGui::ColorEdit3("clear color", reinterpret_cast<float *>(&clear_color));

            if (ImGui::Button("Button"))
            {
                counter++;
            }
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                        1000.0f / io.Framerate, io.Framerate);
            ImGui::End();
        }

        if (show_another_window)
        {
            ImGui::Begin("Another Window", &show_another_window);
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
            {
                show_another_window = false;
            }
            ImGui::End();
        }

        if (show_treemap_window)
        {
            ImGui::Begin("TreeMap Visualization", &show_treemap_window);

            ImGui::Text("Project File TreeMap");
            ImGui::Separator();

            treemap.Render("treemap", ImVec2(0, 400));

            ImGui::Separator();
            ImGui::Text("%s", hovered_info.c_str());
            ImGui::Text("%s", selected_info.c_str());

            ImGui::Separator();
            ImGui::Text("Legend:");
            ImGui::TextColored(ImVec4(70/255.0f, 130/255.0f, 180/255.0f, 1.0f), "■ C++ Files");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(255/255.0f, 140/255.0f, 0/255.0f, 1.0f), "■ Headers");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(60/255.0f, 179/255.0f, 113/255.0f, 1.0f), "■ Docs");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(186/255.0f, 85/255.0f, 211/255.0f, 1.0f), "■ Directories");

            ImGui::End();
        }

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w,
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