#include "ast_matcher_view.h"
#include "ast_metrics.h"
#include "clang/AST/TemplateBase.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"
#include "llvm/Support/raw_ostream.h"
#include <cstring>
#include <iostream>

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

    // Initialize matcher input
    current_matcher_ = "functionDecl()";
    std::strncpy(matcher_input_, current_matcher_.c_str(),
                 sizeof(matcher_input_) - 1);
    matcher_input_[sizeof(matcher_input_) - 1] = '\0';

    initialize_predefined_matchers();
    refresh_analysis();
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

    if (ImGui::Button("Analyze")) {
        source_code_ = std::string(source_buffer_);
        refresh_analysis();
    }
}

void ASTMatcherView::render_matcher_controls()
{
    ImGui::Text("AST Matcher Configuration");

    // Predefined matcher selection
    if (ImGui::BeginCombo(
            "Predefined Matchers",
            selected_predefined_ >= 0
                ? predefined_matchers_[selected_predefined_].name.c_str()
                : "Custom")) {

        for (int i = 0; i < predefined_matchers_.size(); ++i) {
            bool is_selected = (selected_predefined_ == i);
            if (ImGui::Selectable(predefined_matchers_[i].name.c_str(),
                                  is_selected)) {
                selected_predefined_ = i;
                current_matcher_ = predefined_matchers_[i].matcher_code;
                std::strncpy(matcher_input_, current_matcher_.c_str(),
                             sizeof(matcher_input_) - 1);
                matcher_input_[sizeof(matcher_input_) - 1] = '\0';
            }

            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }

            // Show description as tooltip
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s",
                                  predefined_matchers_[i].description.c_str());
            }
        }
        ImGui::EndCombo();
    }

    // Custom matcher input
    if (ImGui::InputText("Matcher Expression", matcher_input_,
                         sizeof(matcher_input_))) {
        current_matcher_ = std::string(matcher_input_);
        selected_predefined_ = -1; // Mark as custom
    }

    ImGui::SameLine();
    if (ImGui::Button("Apply Matcher")) {
        current_matcher_ = std::string(matcher_input_);
        apply_matcher_to_source();
    }

    ImGui::Text("Current Matcher: %s", current_matcher_.c_str());
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
    ImGui::Text("Analysis Statistics");

    if (analysis_result_.has_errors()) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f),
                           "Success Rate: %.1f%% (%zu/%zu nodes)",
                           analysis_result_.success_rate() * 100.0,
                           analysis_result_.nodes_processed -
                               analysis_result_.errors.size(),
                           analysis_result_.nodes_processed);

        if (ImGui::TreeNode("Errors")) {
            for (const auto &error : analysis_result_.errors) {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "• %s",
                                   error.what.c_str());
            }
            ImGui::TreePop();
        }
    } else {
        ImGui::Text("✓ All %zu nodes processed successfully",
                    analysis_result_.nodes_processed);
    }

    ImGui::Text("Functions: %zu, Classes: %zu",
                analysis_result_.functions_found,
                analysis_result_.classes_found);

    if (analysis_result_.max_complexity > 0) {
        ImGui::Text("Complexity: %zu - %zu (total: %zu)",
                    analysis_result_.min_complexity,
                    analysis_result_.max_complexity,
                    analysis_result_.total_complexity);
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

    // Check for template instantiation
    if (func_decl->isTemplateInstantiation()) {
        const auto *temp_spec_info = func_decl->getTemplateSpecializationInfo();
        if (temp_spec_info) {
            ImGui::Separator();
            ImGui::Text("Template Instantiation Details:");
            ImGui::Text(
                "Parameters: %s",
                format_template_parameters(temp_spec_info, ctx).c_str());

            ImGui::Text("Instantiation: %s",
                        format_source_location(
                            ctx.getSourceManager(),
                            temp_spec_info->getPointOfInstantiation())
                            .c_str());
        }
    }
    // Check for explicit template specialization
    else if (func_decl->isFunctionTemplateSpecialization()) {
        const auto *temp_spec_info = func_decl->getTemplateSpecializationInfo();
        if (temp_spec_info) {
            ImGui::Separator();
            ImGui::Text("Template Specialization Details:");
            ImGui::Text(
                "Parameters: %s",
                format_template_parameters(temp_spec_info, ctx).c_str());
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
    ImGui::Text(
        "Location: %s",
        format_source_location(
            analysis_result_.ast_unit->getASTContext().getSourceManager(),
            selected_node_->source_location())
            .c_str());
    ImGui::Text("LOCs: %.1ld", selected_node_->locs());

    // Detailed metrics computed on-demand using direct casting
    const clang::Decl *decl = selected_node_->clang_decl();
    clang::ASTContext &ctx = analysis_result_.ast_unit->getASTContext();

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

void ASTMatcherView::refresh_analysis()
{
    error_message_.clear();
    selected_node_ = nullptr; // Clear selection on new analysis

    try {
        analysis_result_ =
            analyze_with_matcher(source_code_, current_matcher_, filename_);

        if (analysis_result_.root) {
            treemap_ = std::make_unique<TreeMapWidget<ASTNode>>(
                *analysis_result_.root);
            update_coloring_strategy();
            register_treemap_callbacks();
        } else {
            treemap_.reset();
            error_message_ = "Failed to analyze source code with matcher: " +
                             current_matcher_;
        }
    } catch (const std::exception &e) {
        treemap_.reset();
        error_message_ = std::string("Exception during analysis: ") + e.what();
    }
}

bool ASTMatcherView::apply_matcher_to_source()
{
    error_message_.clear();
    selected_node_ = nullptr; // Clear selection on new analysis

    try {
        std::cout << "Applying matcher: " << current_matcher_ << std::endl;

        // Use the real ASTMatcher-based analysis
        analysis_result_ =
            analyze_with_matcher(source_code_, current_matcher_, filename_);

        if (analysis_result_.root) {
            treemap_ = std::make_unique<TreeMapWidget<ASTNode>>(
                *analysis_result_.root);
            update_coloring_strategy();
            register_treemap_callbacks();

            std::cout << "Match analysis completed: "
                      << analysis_result_.functions_found << " functions, "
                      << analysis_result_.classes_found << " classes found"
                      << std::endl;
            return true;
        } else {
            error_message_ =
                "No matches found for expression: " + current_matcher_;
            treemap_.reset();
            return false;
        }
    } catch (const std::exception &e) {
        error_message_ = std::string("Matcher analysis failed: ") + e.what();
        treemap_.reset();
        return false;
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
        treemap_->set_coloring_strategy(
            create_complexity_coloring_strategy(analysis_result_));
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

void ASTMatcherView::initialize_predefined_matchers()
{
    predefined_matchers_ = {
        {"All Functions", "functionDecl()", "Match all function declarations"},
        {"All Classes", "cxxRecordDecl()",
         "Match all C++ class/struct declarations"},
        {"Public Methods", "cxxMethodDecl(isPublic())",
         "Match all public member functions"},
        {"Complex Functions", "functionDecl(hasBody(compoundStmt()))",
         "Match functions with compound statement bodies"},
        {"Constructors", "cxxConstructorDecl()",
         "Match all constructor declarations"},
        {"Virtual Functions", "cxxMethodDecl(isVirtual())",
         "Match all virtual member functions"},
        {"For Loops", "forStmt()", "Match all for loop statements"},
        {"If Statements", "ifStmt()", "Match all if statements"},
        {"Nested Loops", "forStmt(hasDescendant(forStmt()))",
         "Match for loops containing other for loops"}};
}

void ASTMatcherView::set_source_code(const std::string &code,
                                     const std::string &filename)
{
    source_code_ = code;
    filename_ = filename;
    std::strncpy(source_buffer_, code.c_str(), sizeof(source_buffer_) - 1);
    source_buffer_[sizeof(source_buffer_) - 1] = '\0';
    refresh_analysis();
}

void ASTMatcherView::add_predefined_matcher(const std::string &name,
                                            const std::string &description)
{
    // TODO: Allow adding custom predefined matchers
}