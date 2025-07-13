#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <filesystem>
#include "treemap_widget.h"
#include "filesystem_node.h"

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

    // Directory analysis setup
    std::string current_path = std::filesystem::current_path().string();
    char directory_buffer[512];
    strncpy(directory_buffer, current_path.c_str(), sizeof(directory_buffer) - 1);
    directory_buffer[sizeof(directory_buffer) - 1] = '\0';

    int max_depth = 4;
    bool include_hidden = false;
    std::string error_message = "";

    auto filesystem_root = analyze_filesystem(current_path, max_depth, include_hidden);
    std::unique_ptr<TreeMapWidget<FileSystemNode>> treemap =
        std::make_unique<TreeMapWidget<FileSystemNode>>(*filesystem_root);

    // Coloring strategy selection
    enum class ColoringMode
    {
        FileType,
        ModificationTime
    };
    ColoringMode coloring_mode = ColoringMode::FileType;

    auto update_coloring = [&]()
    {
        if (treemap && coloring_mode == ColoringMode::FileType)
        {
            treemap->set_coloring_strategy(file_type_coloring);
        }
        else if (treemap)
        {
            treemap->set_coloring_strategy(modification_time_coloring);
        }
    };

    std::string hovered_info = "Hover over a file to see details";
    std::string selected_info = "Click on a file to select it";

    auto register_callbacks = [&]()
    {
        if (treemap)
        {
            treemap->add_on_node_hover([&](const FileSystemNode &node)
                                       {
                std::string path_info = node.is_directory() ? "Directory: " : "File: ";
                hovered_info = path_info + node.get_relative_path() + " (" + node.format_size() + ")";
                if (!node.is_directory()) {
                    hovered_info += " - Modified " + std::to_string(static_cast<int>(node.days_since_modified())) + " days ago";
                } });

            treemap->add_on_node_click([&](const FileSystemNode &node)
                                       {
                std::string path_info = node.is_directory() ? "Selected Directory: " : "Selected File: ";
                selected_info = path_info + node.get_relative_path() + " (" + node.format_size() + ")"; });
        }
    };

    auto refresh_directory = [&]()
    {
        error_message = "";
        try
        {
            std::filesystem::path path(directory_buffer);
            if (!std::filesystem::exists(path))
            {
                error_message = "Directory does not exist: " + std::string(directory_buffer);
                return false;
            }
            if (!std::filesystem::is_directory(path))
            {
                error_message = "Path is not a directory: " + std::string(directory_buffer);
                return false;
            }

            current_path = std::filesystem::canonical(path).string();
            filesystem_root = analyze_filesystem(current_path, max_depth, include_hidden);
            treemap = std::make_unique<TreeMapWidget<FileSystemNode>>(*filesystem_root);
            update_coloring();
            register_callbacks();

            return true;
        }
        catch (const std::exception &e)
        {
            error_message = "Error analyzing directory: " + std::string(e.what());
            return false;
        }
    };

    update_coloring();
    register_callbacks();

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

            ImGui::Begin("Prism Control Panel");

            ImGui::Text("Welcome to Prism - Code Analysis Tool");
            ImGui::Separator();

            ImGui::Checkbox("Demo Window", &show_demo_window);
            ImGui::Checkbox("Another Window", &show_another_window);
            ImGui::Checkbox("TreeMap Window", &show_treemap_window);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
            ImGui::ColorEdit3("clear color", reinterpret_cast<float *>(&clear_color));

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                        1000.0f / io.Framerate, io.Framerate);
            ImGui::End();
        }

        if (show_treemap_window)
        {
            ImGui::Begin("Filesystem TreeMap", &show_treemap_window);

            // Directory input section
            ImGui::Text("Directory Path:");
            ImGui::SetNextItemWidth(-120); // Leave space for Browse button
            bool path_changed = ImGui::InputText("##directory", directory_buffer, sizeof(directory_buffer), ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::SameLine();
            bool browse_clicked = ImGui::Button("Browse...");
            ImGui::SameLine();
            bool refresh_clicked = ImGui::Button("Refresh");

            // Directory options
            ImGui::Text("Options:");
            bool depth_changed = ImGui::SliderInt("Max Depth", &max_depth, 1, 8);
            ImGui::SameLine();
            bool hidden_changed = ImGui::Checkbox("Include Hidden", &include_hidden);

            // Handle directory changes
            if (path_changed || browse_clicked || refresh_clicked || depth_changed || hidden_changed)
            {
                if (browse_clicked)
                {
                    // For now, just use the current path - a proper file dialog would need platform-specific code
                    // You could integrate with libraries like nativefiledialog or tinyfiledialogs here
                    std::string home_dir = std::getenv("HOME") ? std::getenv("HOME") : "/tmp";
                    strncpy(directory_buffer, home_dir.c_str(), sizeof(directory_buffer) - 1);
                    directory_buffer[sizeof(directory_buffer) - 1] = '\0';
                }
                refresh_directory();
            }

            // Show error message if any
            if (!error_message.empty())
            {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Error: %s", error_message.c_str());
            }

            ImGui::Text("Current Directory: %s", current_path.c_str());
            ImGui::Separator();

            // Coloring mode selection
            ImGui::Text("Coloring Mode:");
            if (ImGui::RadioButton("File Type", coloring_mode == ColoringMode::FileType))
            {
                coloring_mode = ColoringMode::FileType;
                update_coloring();
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("Modification Time", coloring_mode == ColoringMode::ModificationTime))
            {
                coloring_mode = ColoringMode::ModificationTime;
                update_coloring();
            }

            ImGui::Separator();

            if (treemap)
            {
                treemap->Render("treemap", ImVec2(0, 300));
            }
            else
            {
                ImGui::Text("No data available - please select a valid directory");
            }

            ImGui::Separator();
            ImGui::Text("%s", hovered_info.c_str());
            ImGui::Text("%s", selected_info.c_str());

            ImGui::Separator();
            if (coloring_mode == ColoringMode::FileType)
            {
                ImGui::Text("File Type Legend:");
                ImGui::TextColored(ImVec4(70 / 255.0f, 130 / 255.0f, 180 / 255.0f, 1.0f), "■ C++");
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(255 / 255.0f, 140 / 255.0f, 0 / 255.0f, 1.0f), "■ Headers");
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(60 / 255.0f, 179 / 255.0f, 113 / 255.0f, 1.0f), "■ Docs");
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(186 / 255.0f, 85 / 255.0f, 211 / 255.0f, 1.0f), "■ Directories");

                ImGui::TextColored(ImVec4(255 / 255.0f, 215 / 255.0f, 0 / 255.0f, 1.0f), "■ Python");
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(240 / 255.0f, 230 / 255.0f, 140 / 255.0f, 1.0f), "■ JS/TS");
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(255 / 255.0f, 182 / 255.0f, 193 / 255.0f, 1.0f), "■ Config");
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(220 / 255.0f, 220 / 255.0f, 220 / 255.0f, 1.0f), "■ Other");
            }
            else
            {
                ImGui::Text("Modification Time Legend:");
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "■ Fresh (< 1 day)");
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1, 1, 0, 1), "■ Recent (1-7 days)");
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "■ Medium (7-30 days)");
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "■ Old (30+ days)");
            }

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