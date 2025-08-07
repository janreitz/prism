#include <clang/Frontend/ASTUnit.h>
#include <clang/Tooling/CompilationDatabase.h>

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace prism::ast_generation
{
std::unique_ptr<clang::ASTUnit>
parse_ast_from_string(const std::string &source_code,
                      const std::vector<std::string> &args,
                      const std::string &file_name);

using ProgressCallback = std::function<void(int completed, int total,
                                            const std::string &current_file)>;
using ErrorCallback = std::function<void(const std::string &error_message)>;

std::vector<std::unique_ptr<clang::ASTUnit>> parse_project_asts(
    const clang::tooling::CompilationDatabase &compilation_db,
    const std::vector<std::string> &source_files,
    std::optional<ProgressCallback> progress_callback = std::nullopt,
    std::optional<ErrorCallback> error_callback = std::nullopt);
} // namespace prism::ast_generation