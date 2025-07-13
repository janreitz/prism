#include "filesystem_node.h"
#include "treemap_widget.h"
#include <GLFW/glfw3.h>
#include <filesystem>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>

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
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

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
    strncpy(directory_buffer, current_path.c_str(),
            sizeof(directory_buffer) - 1);
    directory_buffer[sizeof(directory_buffer) - 1] = '\0';

    int max_depth = 4;
    bool include_hidden = false;
    std::string error_message = "";

    // Coloring strategy selection
    enum class ColoringMode { FileType, ModificationTime };
    ColoringMode coloring_mode = ColoringMode::FileType;
    ColoringContext current_context;
    AnalysisResult current_analysis;

    current_analysis = analyze_filesystem_with_errors(current_path, max_depth, include_hidden);
    auto filesystem_root = std::move(current_analysis.root);
    std::unique_ptr<TreeMapWidget<FileSystemNode>> treemap =
        filesystem_root ? std::make_unique<TreeMapWidget<FileSystemNode>>(*filesystem_root) : nullptr;

    auto update_coloring = [&]() {
        if (!treemap)
            return;

        // Analyze the current tree for context
        current_context = analyze_coloring_context(*filesystem_root);

        if (coloring_mode == ColoringMode::FileType) {
            auto strategy = create_balanced_extension_strategy(current_context);
            treemap->set_coloring_strategy(strategy);
        } else {
            auto strategy = create_relative_time_strategy(current_context);
            treemap->set_coloring_strategy(strategy);
        }
    };

    std::string hovered_info = "Hover over a file to see details";
    std::string selected_info = "Click on a file to select it";

    auto register_callbacks = [&]() {
        if (treemap) {
            treemap->add_on_node_hover([&](const FileSystemNode &node) {
                std::string path_info = node.is_directory() ? "Directory: " : "File: ";
                hovered_info = path_info + node.get_relative_path() + " (" + node.format_size() + ")";
                if (!node.is_directory()) {
                    hovered_info += " - Modified " + std::to_string(static_cast<int>(node.days_since_modified())) + " days ago";
                }
            });

            treemap->add_on_node_click([&](const FileSystemNode &node) {
                std::string path_info = node.is_directory() ? "Selected Directory: " : "Selected File: ";
                selected_info = path_info + node.get_relative_path() + " (" + node.format_size() + ")";
            });
        }
    };

    auto refresh_directory = [&]() {
        error_message = "";
        try {
            std::filesystem::path path(directory_buffer);
            if (!std::filesystem::exists(path)) {
                error_message = "Directory does not exist: " +
                                std::string(directory_buffer);
                return false;
            }
            if (!std::filesystem::is_directory(path)) {
                error_message =
                    "Path is not a directory: " + std::string(directory_buffer);
                return false;
            }

            current_path = std::filesystem::canonical(path).string();
            current_analysis = analyze_filesystem_with_errors(current_path, max_depth, include_hidden);
            filesystem_root = std::move(current_analysis.root);
            treemap = filesystem_root ? std::make_unique<TreeMapWidget<FileSystemNode>>(*filesystem_root) : nullptr;
            update_coloring();
            register_callbacks();

            return true;
        } catch (const std::exception &e) {
            error_message =
                "Error analyzing directory: " + std::string(e.what());
            return false;
        }
    };

    update_coloring();
    register_callbacks();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Enable docking
        ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);

        if (show_demo_window) {
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
            ImGui::ColorEdit3("clear color",
                              reinterpret_cast<float *>(&clear_color));

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                        1000.0f / io.Framerate, io.Framerate);
            ImGui::End();
        }

        if (show_treemap_window) {
            ImGui::Begin("Filesystem TreeMap", &show_treemap_window);

            // Directory input section
            ImGui::Text("Directory Path:");
            ImGui::SetNextItemWidth(-120); // Leave space for Browse button
            bool path_changed = ImGui::InputText(
                "##directory", directory_buffer, sizeof(directory_buffer),
                ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::SameLine();
            bool browse_clicked = ImGui::Button("Browse...");
            ImGui::SameLine();
            bool refresh_clicked = ImGui::Button("Refresh");

            // Directory options
            ImGui::Text("Options:");
            bool depth_changed =
                ImGui::SliderInt("Max Depth", &max_depth, 1, 8);
            ImGui::SameLine();
            bool hidden_changed =
                ImGui::Checkbox("Include Hidden", &include_hidden);

            // Handle directory changes
            if (path_changed || browse_clicked || refresh_clicked ||
                depth_changed || hidden_changed) {
                if (browse_clicked) {
                    // For now, just use the current path - a proper file dialog
                    // would need platform-specific code You could integrate
                    // with libraries like nativefiledialog or tinyfiledialogs
                    // here
                    std::string home_dir =
                        std::getenv("HOME") ? std::getenv("HOME") : "/tmp";
                    strncpy(directory_buffer, home_dir.c_str(),
                            sizeof(directory_buffer) - 1);
                    directory_buffer[sizeof(directory_buffer) - 1] = '\0';
                }
                refresh_directory();
            }

            // Show error message if any
            if (!error_message.empty()) {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Error: %s",
                                   error_message.c_str());
            }
            
            // Show filesystem analysis diagnostics
            if (current_analysis.has_errors()) {
                ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), 
                    "Warning: %zu/%zu files inaccessible (%.1f%% success)", 
                    current_analysis.errors.size(), 
                    current_analysis.total_attempted,
                    current_analysis.success_rate() * 100.0);
                
                if (ImGui::IsItemHovered() && ImGui::BeginTooltip()) {
                    ImGui::Text("Inaccessible files:");
                    int max_show = std::min(10, static_cast<int>(current_analysis.errors.size()));
                    for (int i = 0; i < max_show; ++i) {
                        ImGui::Text("• %s", current_analysis.errors[i].what.c_str());
                    }
                    if (current_analysis.errors.size() > max_show) {
                        ImGui::Text("... and %zu more", current_analysis.errors.size() - max_show);
                    }
                    ImGui::EndTooltip();
                }
            } else if (current_analysis.total_attempted > 0) {
                ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), 
                    "✓ All %zu files accessible", current_analysis.successful_nodes);
            }

            ImGui::Text("Current Directory: %s", current_path.c_str());
            ImGui::Separator();

            // Coloring mode selection
            ImGui::Text("Coloring Mode:");
            if (ImGui::RadioButton("File Type",
                                   coloring_mode == ColoringMode::FileType)) {
                coloring_mode = ColoringMode::FileType;
                update_coloring();
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("Modification Time",
                                   coloring_mode ==
                                       ColoringMode::ModificationTime)) {
                coloring_mode = ColoringMode::ModificationTime;
                update_coloring();
            }

            ImGui::Separator();

            if (treemap) {
                treemap->Render("treemap", ImVec2(0, 300));
            } else {
                ImGui::Text(
                    "No data available - please select a valid directory");
            }

            ImGui::Separator();
            ImGui::Text("%s", hovered_info.c_str());
            ImGui::Text("%s", selected_info.c_str());

            ImGui::Separator();
            if (coloring_mode == ColoringMode::FileType) {
                ImGui::Text("File Extensions Found (%zu types):",
                            current_context.unique_extensions.size());

                // Show directories first
                if (current_context.directory_count > 0) {
                    ImGui::TextColored(
                        ImVec4(186 / 255.0f, 85 / 255.0f, 211 / 255.0f, 1.0f),
                        "■ Directories (%d)", current_context.directory_count);
                    if (current_context.unique_extensions.size() > 0)
                        ImGui::SameLine();
                }

                // Show file extensions with their assigned colors and counts
                for (size_t i = 0; i < current_context.unique_extensions.size();
                     ++i) {
                    const std::string &ext =
                        current_context.unique_extensions[i];
                    int count = current_context.extension_counts.at(ext);

                    // Generate the same color as the strategy
                    float hue =
                        (360.0f * i) / current_context.unique_extensions.size();
                    float saturation = 0.75f;
                    float value = 0.85f;
                    ImU32 color = hsv_to_rgb(hue, saturation, value);

                    float r = ((color >> 0) & 0xFF) / 255.0f;
                    float g = ((color >> 8) & 0xFF) / 255.0f;
                    float b = ((color >> 16) & 0xFF) / 255.0f;

                    std::string display_ext =
                        ext.empty() ? "no extension" : ext;
                    ImGui::TextColored(ImVec4(r, g, b, 1.0f), "■ %s (%d)",
                                       display_ext.c_str(), count);

                    // Layout in rows of 4
                    if ((i + 1) % 4 != 0 &&
                        i + 1 < current_context.unique_extensions.size()) {
                        ImGui::SameLine();
                    }
                }
            } else {
                if (current_context.has_data()) {
                    ImGui::Text(
                        "Modification Time Range (Relative to Directory):");
                    ImGui::Text("Newest: %.1f days ago",
                                current_context.min_days_since_modified);
                    ImGui::Text("Oldest: %.1f days ago",
                                current_context.max_days_since_modified);

                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(0, 1, 0, 1), "■ Newest files");
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1, 1, 0, 1), "■ Medium age");
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1, 0, 0, 1), "■ Oldest files");
                    ImGui::SameLine();
                    ImGui::TextColored(
                        ImVec4(186 / 255.0f, 85 / 255.0f, 211 / 255.0f, 1.0f),
                        "■ Directories");
                } else {
                    ImGui::Text("No modification time data available");
                }
            }

            ImGui::End();
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