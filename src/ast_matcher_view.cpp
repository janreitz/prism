#include "ast_matcher_view.h"
#include "ast_generation.h"
#include "ast_metrics.h"
#include "clang/AST/TemplateBase.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Tooling/CompilationDatabase.h"
#include "llvm/Support/raw_ostream.h"
#include <cstring>
#include <expected>
#include <iostream>
#include <regex>

ASTMatcherView::ASTMatcherView()
{
    // Initialize with example source code that demonstrates structural
    // hierarchy
    source_code_ = R"(
#include <iostream>
#include <vector>
#include <memory>
#include <cmath>

namespace graphics {
    namespace math {
        class Vector3 {
        private:
            float x_, y_, z_;
            
        public:
            Vector3(float x, float y, float z) : x_(x), y_(y), z_(z) {}
            
            float x() const { return x_; }
            float y() const { return y_; }
            float z() const { return z_; }
            
            float length() const {
                return std::sqrt(x_ * x_ + y_ * y_ + z_ * z_);
            }
            
            Vector3 normalize() const {
                float len = length();
                if (len > 0) {
                    return Vector3(x_ / len, y_ / len, z_ / len);
                }
                return Vector3(0, 0, 0);
            }
        };
        
        float dot(const Vector3& a, const Vector3& b) {
            return a.x() * b.x() + a.y() * b.y() + a.z() * b.z();
        }
    }
    
    class Renderer {
    protected:
        std::vector<math::Vector3> vertices_;
        bool initialized_;
        
    public:
        Renderer() : initialized_(false) {}
        virtual ~Renderer() = default;
        
        virtual void initialize() {
            vertices_.reserve(1000);
            initialized_ = true;
        }
        
        void addVertex(const math::Vector3& vertex) {
            if (initialized_) {
                vertices_.push_back(vertex);
            }
        }
        
        virtual void render() = 0;
        
        size_t getVertexCount() const { return vertices_.size(); }
    };
    
    class OpenGLRenderer : public Renderer {
    public:
        void render() override {
            if (!getVertexCount()) return;
            
            for (const auto& vertex : vertices_) {
                // Render vertex with OpenGL
                renderVertex(vertex);
            }
        }
        
    private:
        void renderVertex(const math::Vector3& vertex) {
            // OpenGL-specific rendering code
            float x = vertex.x();
            float y = vertex.y(); 
            float z = vertex.z();
        }
    };
}

namespace utils {
    template<typename T>
    class Logger {
    private:
        std::vector<T> logs_;
        
    public:
        void log(const T& message) {
            logs_.push_back(message);
            if (logs_.size() > 100) {
                logs_.erase(logs_.begin());
            }
        }
        
        void clear() { logs_.clear(); }
        size_t size() const { return logs_.size(); }
    };
}

int main() {
    graphics::math::Vector3 pos(1.0f, 2.0f, 3.0f);
    auto renderer = std::make_unique<graphics::OpenGLRenderer>();
    renderer->initialize();
    renderer->addVertex(pos);
    
    utils::Logger<std::string> logger;
    logger.log("Application started");
    
    return 0;
}
)";

    std::strncpy(source_buffer_, source_code_.c_str(),
                 sizeof(source_buffer_) - 1);
    source_buffer_[sizeof(source_buffer_) - 1] = '\0';
}

bool ASTMatcherView::render()
{
    bool keep_open = true;

    if (ImGui::Begin("AST Matcher Analysis", &keep_open)) {
        render_source_input();
        ImGui::Separator();
        render_matcher_controls();

        if (treemap_) {
            ImGui::Separator();
            render_treemap();
            render_interactive_info();
            ImGui::Separator();
            render_selection_details();
            ImGui::Separator();
            render_statistics();
        } else if (!error_message_.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Error: %s",
                               error_message_.c_str());
        }
    }
    ImGui::End();

    return keep_open;
}

void ASTMatcherView::render_source_input()
{
    ImGui::Text("AST Source Selection");

    // Mode selection
    static int input_mode = 0; // 0 = string, 1 = project
    ImGui::RadioButton("Source Code", &input_mode, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Project Directory", &input_mode, 1);

    ImGui::Separator();

    if (input_mode == 0) {
        render_string_input();
    } else {
        render_project_input();
    }
    if (!ast_units_.empty()) {
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "Parsed %zu translation units",
                           ast_units_.size());
    }
}

void ASTMatcherView::render_string_input()
{
    ImGui::Text("Source Code Input");

    static char filename_buffer[256];
    std::strncpy(filename_buffer, filename_.c_str(),
                 sizeof(filename_buffer) - 1);
    filename_buffer[sizeof(filename_buffer) - 1] = '\0';

    if (ImGui::InputText("Filename", filename_buffer,
                         sizeof(filename_buffer))) {
        filename_ = std::string(filename_buffer);
    }

    if (ImGui::InputTextMultiline("##source", source_buffer_,
                                  sizeof(source_buffer_), ImVec2(-1, 100))) {
        source_code_ = std::string(source_buffer_);
    }

    if (ImGui::Button("Parse AST")) {
        source_code_ = std::string(source_buffer_);
        error_message_.clear();
        selected_node_ = nullptr;
        auto new_ast_unit = prism::ast_generation::parse_ast_from_string(
            source_code_, args_, filename_);
        if (new_ast_unit) {
            ast_units_.clear();
            ast_units_.push_back(std::move(new_ast_unit));
        }
    }
}

void ASTMatcherView::render_project_input()
{
    static char project_root_buffer[512] = "";
    ImGui::InputText("Project Build Directory", project_root_buffer,
                     sizeof(project_root_buffer));

    std::filesystem::path project_root(project_root_buffer);
    // Only enable button if we have a valid directory
    bool project_root_is_valid = std::filesystem::exists(project_root) &&
                                 std::filesystem::is_directory(project_root);

    if (project_root_is_valid) {
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "Valid directory");
    } else {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Invalid directory");
    }

    if (!project_root_is_valid) {
        ImGui::BeginDisabled();
    }

    if (ImGui::Button("Load compile_commands.json")) {
        std::string error_message;
        compilation_db_ =
            clang::tooling::CompilationDatabase::autoDetectFromDirectory(
                project_root.string(), error_message);

        if (!compilation_db_) {
            error_message_ =
                "Failed to detect compilation database: " + error_message;
            std::cout << error_message_ << std::endl;
        } else {
            error_message_.clear();
            std::cout << "Successfully loaded compilation database"
                      << std::endl;
        }
    }

    if (!project_root_is_valid) {
        ImGui::EndDisabled();
    }

    if (compilation_db_) {
        source_files_ = compilation_db_->getAllFiles();
        const std::string node_label = "Compilation database loaded, found " +
                                       std::to_string(source_files_.size()) +
                                       " source files";

        if (ImGui::TreeNode(node_label.c_str())) {
            ImGui::Indent();

            std::sort(source_files_.begin(), source_files_.end());

            for (const auto &file : source_files_) {
                ImGui::Text("%s", file.c_str());
            }

            ImGui::Unindent();
            ImGui::TreePop();
        }

        static std::array<char, 256> filter_expression;
        ImGui::InputText("Filter files", filter_expression.data(),
                         sizeof(filter_expression));

        // apply_filter
        const auto maybe_pattern =
            []() -> std::expected<std::regex, std::string> {
            try {
                return std::regex(filter_expression.data(),
                                  std::regex_constants::icase);

            } catch (const std::regex_error &e) {
                return std::unexpected("Error parsing regex: \"" +
                                       std::string(filter_expression.data()) +
                                       "\": " + e.what());
            }
        }();

        if (maybe_pattern.has_value()) {
            const auto pattern = maybe_pattern.value();
            auto remove_iter =
                std::remove_if(source_files_.begin(), source_files_.end(),
                               [&pattern](const std::string &file) {
                                   return !std::regex_search(file, pattern);
                               });
            source_files_.erase(remove_iter, source_files_.end());
        } else {
            ImGui::Text("%s", maybe_pattern.error().c_str());
        }

        const std::string filter_result_label =
            "Filter results in " + std::to_string(source_files_.size()) +
            " source files";
        if (ImGui::TreeNode(filter_result_label.c_str())) {
            ImGui::Indent();

            for (const auto &file : source_files_) {
                ImGui::Text("%s", file.c_str());
            }

            ImGui::Unindent();
            ImGui::TreePop();
        }
    }

    const bool ast_parsing_possible = compilation_db_ && !source_files_.empty();

    if (!ast_parsing_possible) {
        ImGui::BeginDisabled();
    }

    if (ImGui::Button("Parse ASTs")) {
        selected_node_ = nullptr;
        ast_units_ = prism::ast_generation::parse_project_asts(*compilation_db_,
                                                               source_files_);
    }
    if (!ast_parsing_possible) {
        ImGui::EndDisabled();
    }
}

void ASTMatcherView::render_matcher_controls()
{
    ImGui::Text("AST Matcher Configuration");
    if (ImGui::BeginCombo(
            "Predefined Matchers",
            predefined_matchers[current_matcher_idx_].first.c_str())) {

        for (size_t i = 0; i < predefined_matchers.size(); ++i) {
            const bool is_selected = (current_matcher_idx_ == i);
            if (ImGui::Selectable(predefined_matchers[i].first.c_str(),
                                  is_selected)) {
                current_matcher_idx_ = i;
                apply_matcher_to_source();
            }

            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
}

bool ASTMatcherView::apply_matcher_to_source()
{
    if (ast_units_.empty()) {
        std::cout << "Need to parse AST before matching!" << std::endl;
        return false;
    }
    error_message_.clear();
    selected_node_ = nullptr; // Clear selection on new analysis

    try {
        std::cout << "Applying matcher: " << current_matcher_idx_ << std::endl;

        analysis_result_ = analyze_with_matcher(
            ast_units_.front()->getASTContext(),
            predefined_matchers[current_matcher_idx_].second, filename_);

        if (analysis_result_.has_value()) {
            const auto &analysis_result = analysis_result_.value();
            treemap_ =
                std::make_unique<TreeMapWidget<ASTNode>>(*analysis_result.root);
            update_coloring_strategy();
            register_treemap_callbacks();

            std::cout << "Match analysis completed: "
                      << analysis_result.functions_found << " functions, "
                      << analysis_result.classes_found << " classes found"
                      << std::endl;
            return true;
        } else {
            error_message_ =
                "No matches found for expression: " + current_matcher_idx_;
            treemap_.reset();
            return false;
        }
    } catch (const std::exception &e) {
        error_message_ = std::string("Matcher analysis failed: ") + e.what();
        treemap_.reset();
        return false;
    }
}

void ASTMatcherView::render_treemap()
{
    // Coloring mode selection
    ImGui::Text("Coloring Strategy:");
    ImGui::SameLine();
    if (ImGui::RadioButton("Node Type",
                           coloring_mode_ == ColoringMode::NodeType)) {
        coloring_mode_ = ColoringMode::NodeType;
        update_coloring_strategy();
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Complexity",
                           coloring_mode_ == ColoringMode::Complexity)) {
        coloring_mode_ = ColoringMode::Complexity;
        update_coloring_strategy();
    }

    ImVec2 available_size = ImGui::GetContentRegionAvail();
    available_size.y =
        std::max(available_size.y - 250.0f,
                 200.0f); // Reserve space for info and statistics

    treemap_->render("AST TreeMap", available_size, false);
}

void ASTMatcherView::render_interactive_info()
{
    ImGui::Text("%s", hovered_info_.c_str());
}

void ASTMatcherView::render_statistics()
{
    if (!analysis_result_.has_value()) {
        return;
    }
    const auto &analysis_result = analysis_result_.value();
    ImGui::Text("Analysis Statistics");

    if (analysis_result.has_errors()) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f),
                           "Success Rate: %.1f%% (%zu/%zu nodes)",
                           analysis_result.success_rate() * 100.0,
                           analysis_result.nodes_processed -
                               analysis_result.errors.size(),
                           analysis_result.nodes_processed);

        if (ImGui::TreeNode("Errors")) {
            for (const auto &error : analysis_result.errors) {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "• %s",
                                   error.what.c_str());
            }
            ImGui::TreePop();
        }
    } else {
        ImGui::Text("✓ All %zu nodes processed successfully",
                    analysis_result.nodes_processed);
    }

    ImGui::Text("Functions: %zu, Classes: %zu", analysis_result.functions_found,
                analysis_result.classes_found);

    if (analysis_result.max_complexity > 0) {
        ImGui::Text("Complexity: %zu - %zu (total: %zu)",
                    analysis_result.min_complexity,
                    analysis_result.max_complexity,
                    analysis_result.total_complexity);
    }
}

std::string format_template_parameters(
    const clang::FunctionTemplateSpecializationInfo *spec_info,
    const clang::ASTContext &ctx)
{
    std::string template_params;
    llvm::raw_string_ostream out(template_params);
    // Try to get template argument information
    if (const auto *args = spec_info->TemplateArguments) {
        for (unsigned i = 0; i < args->size(); ++i) {
            if (i > 0)
                out << ", ";
            const auto &arg = args->get(i);
            arg.dump(out);
        }
    }
    return template_params;
}

void render_function_details(const clang::FunctionDecl *func_decl,
                             clang::ASTContext &ctx)
{
    auto metrics = compute_function_metrics(func_decl, ctx);
    ImGui::Text("Function Metrics:");
    ImGui::Text("  Statement Count: %zu", metrics.statement_count);
    ImGui::Text("  Parameter Count: %zu", metrics.parameter_count);
    ImGui::Text("  Cyclomatic Complexity: %zu", metrics.cyclomatic_complexity);

    // Check if this is any kind of template specialization (clang considers
    // instantiation as specializations)

    if (func_decl->isFunctionTemplateSpecialization()) {
        const auto *temp_spec_info = func_decl->getTemplateSpecializationInfo();
        if (temp_spec_info) {
            ImGui::Separator();

            bool isImplicit = temp_spec_info->getTemplateSpecializationKind() ==
                              clang::TSK_ImplicitInstantiation;

            if (isImplicit) {
                ImGui::Text("Template Instantiation Details:");
            } else {
                ImGui::Text("Template Specialization Details:");
            }

            // Template parameters
            ImGui::Text(
                "Parameters: %s",
                format_template_parameters(temp_spec_info, ctx).c_str());

            // Point of instantiation (for implicit instantiations)
            if (isImplicit) {
                ImGui::Text("Instantiation: %s",
                            format_source_location(
                                ctx.getSourceManager(),
                                temp_spec_info->getPointOfInstantiation())
                                .c_str());
            }

            if (auto *primary_template = temp_spec_info->getTemplate()) {
                ImGui::Text(
                    "Primary Template: %s",
                    format_source_location(ctx.getSourceManager(),
                                           primary_template->getLocation())
                        .c_str());
            }
        }
    }
}

void ASTMatcherView::render_selection_details()
{
    if (!selected_node_) {
        ImGui::Text(
            "Click on a node in the treemap to see detailed information");
        return;
    }

    ImGui::Text("Selected Node Details");

    // Basic info
    ImGui::Text("Name: %s", selected_node_->get_qualified_name().c_str());
    ImGui::Text("Type: %s", selected_node_->type_string().c_str());
    ImGui::Text("Location: %s",
                format_source_location(
                    ast_units_.front()->getASTContext().getSourceManager(),
                    selected_node_->source_location())
                    .c_str());
    ImGui::Text("LOCs: %.1ld", selected_node_->locs());

    // Detailed metrics computed on-demand using direct casting
    const clang::Decl *decl = selected_node_->clang_decl();
    clang::ASTContext &ctx = ast_units_.front()->getASTContext();

    if (const auto *func_decl = clang::dyn_cast<clang::FunctionDecl>(decl)) {
        render_function_details(func_decl, ctx);
    } else if (const auto *class_decl =
                   clang::dyn_cast<clang::CXXRecordDecl>(decl)) {
        auto metrics = compute_class_metrics(class_decl, ctx);
        ImGui::Text("Class Metrics:");
        ImGui::Text("  Total Members: %zu", metrics.member_count);
        ImGui::Text("  Methods: %zu", metrics.method_count);
        ImGui::Text("  Public Members: %zu", metrics.public_member_count);
        ImGui::Text("  Private Members: %zu", metrics.private_member_count);
    } else if (clang::isa<clang::NamespaceDecl>(decl)) {
        auto metrics = compute_namespace_metrics(
            decl, ctx, selected_node_->children().size());
        ImGui::Text("Namespace Metrics:");
        ImGui::Text("  Child Count: %zu", metrics.child_count);
    } else {
        ImGui::Text("Type not implemented");
    }
}

void ASTMatcherView::update_coloring_strategy()
{
    if (!treemap_)
        return;

    switch (coloring_mode_) {
    case ColoringMode::NodeType:
        treemap_->set_coloring_strategy(create_type_based_coloring_strategy());
        break;
    case ColoringMode::Complexity:
        treemap_->set_coloring_strategy(create_complexity_coloring_strategy(
            analysis_result_.value(), ast_units_.front().get()));
        break;
    }
}

void ASTMatcherView::register_treemap_callbacks()
{
    if (!treemap_)
        return;

    treemap_->add_on_node_hover([this](const ASTNode &node) {
        hovered_info_ = node.type_string() + ": " + node.get_qualified_name();
    });

    treemap_->add_on_node_click([this](const ASTNode &node) {
        selected_node_ = &node;
        std::cout << "Selected AST node: " << node.get_qualified_name() << " ("
                  << node.type_string() << ")" << std::endl;
    });
}
