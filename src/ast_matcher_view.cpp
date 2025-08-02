#include "ast_matcher_view.h"
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
            ImGui::Separator();
            render_interactive_info();
            ImGui::Separator();
            render_statistics();
            ImGui::Separator();
            render_match_results();
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

    // Coloring mode selection
    ImGui::Text("Coloring Strategy:");
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
}

void ASTMatcherView::render_treemap()
{
    ImVec2 available_size = ImGui::GetContentRegionAvail();
    available_size.y =
        std::max(available_size.y - 250.0f,
                 200.0f); // Reserve space for info and statistics

    treemap_->render("AST TreeMap", available_size, false);
}

void ASTMatcherView::render_interactive_info()
{
    ImGui::Text("%s", hovered_info_.c_str());
    ImGui::Text("%s", selected_info_.c_str());
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

void ASTMatcherView::render_match_results()
{
    ImGui::Text("Current Matcher: %s", current_matcher_.c_str());

    // TODO: Add table view of matched nodes with details
    if (ImGui::TreeNode("Match Details")) {
        ImGui::Text("Matched nodes will be listed here");
        ImGui::Text("- Node type, location, metrics");
        ImGui::Text("- Click to highlight in treemap");
        ImGui::TreePop();
    }
}

void ASTMatcherView::refresh_analysis()
{
    error_message_.clear();

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

    try {
        std::cout << "Applying matcher: " << current_matcher_ << std::endl;

        // Validate the matcher expression
        if (!validate_matcher_expression(current_matcher_)) {
            error_message_ = "Invalid matcher expression: " + current_matcher_;
            return false;
        }

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
        size_t loc = 0;
        size_t complexity = 1;

        // Extract metrics from variant
        std::visit(
            [&loc, &complexity](const auto &metrics) {
                loc = metrics.lines_of_code;
                if constexpr (requires { metrics.cyclomatic_complexity; }) {
                    complexity = metrics.cyclomatic_complexity;
                }
            },
            node.metrics());

        hovered_info_ = node.type_string() + ": " + node.get_qualified_name() +
                        " (LOC: " + std::to_string(loc) +
                        ", Complexity: " + std::to_string(complexity) + ")";
    });

    treemap_->add_on_node_click([this](const ASTNode &node) {
        selected_info_ = "Selected " + node.type_string() + ": " +
                         node.get_qualified_name() +
                         " (Size: " + std::to_string(node.size()) + ")";
        std::cout << "Clicked AST node: " << node.name() << " ("
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