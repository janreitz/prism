#include "ast_generation.h"

#include "clang/Tooling/Tooling.h"

#include <string>
#include <vector>

namespace prism::ast_generation
{
std::unique_ptr<clang::ASTUnit>
parse_ast_from_string(const std::string &source_code,
                      const std::vector<std::string> &args,
                      const std::string &file_name)
{
    return clang::tooling::buildASTFromCodeWithArgs(source_code, args,
                                                    file_name);
}

std::vector<std::unique_ptr<clang::ASTUnit>>
parse_project_ast(const clang::tooling::CompilationDatabase &compilation_db,
                  const std::filesystem::path &project_root)
{

    clang::tooling::ClangTool tool(compilation_db,
                                   compilation_db.getAllFiles());

    std::vector<std::unique_ptr<clang::ASTUnit>> ast_units;
    tool.buildASTs(ast_units);
    return ast_units;
}

} // namespace prism::ast_generation