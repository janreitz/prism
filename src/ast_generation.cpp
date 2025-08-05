#include "ast_generation.h"

#include <clang/Basic/Diagnostic.h>
#include <clang/Tooling/Tooling.h>

#include "llvm/ADT/STLExtras.h"

#include <iostream>
#include <source_location>
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

class MyDiagnosticConsumer : public clang::DiagnosticConsumer
{
  public:
    //   Callback to inform the diagnostic client that processing of a source
    //   file is beginning.
    void BeginSourceFile(const clang::LangOptions &LangOpts,
                         const clang::Preprocessor *PP = nullptr) override
    {
        std::cerr << "Starting to process file number " << file_count_++
                  << std::endl;

        if (PP) {
            auto &SM = PP->getSourceManager();
            auto FID = SM.getMainFileID();
            if (FID.isValid()) {
                if (auto *Entry = SM.getFileEntryForID(FID)) {
                    std::cerr << "Processing file: " << Entry->getName().data()
                              << std::endl;
                }
            } else {
                std::cerr << "MainFileID invalid" << std::endl;
            }
        } else {
            std::cerr << std::source_location().function_name()
                      << "(no preprocessor)" << std::endl;
        }
    }

    // Callback to inform the diagnostic client that processing of a source
    // file has ended.
    void EndSourceFile() override {}

    // Callback to inform the diagnostic client that processing of all
    // source files has ended.
    void finish() override
    {
        std::cerr << std::source_location().function_name();
    }

    // Indicates whether the diagnostics handled by this DiagnosticConsumer
    // should be included in the number of diagnostics reported by
    // DiagnosticsEngine.
    bool IncludeInDiagnosticCounts() const override { return false; }

    // Handle this diagnostic, reporting it to the user or capturing it to a
    // log as needed.
    void HandleDiagnostic(clang::DiagnosticsEngine::Level DiagLevel,
                          const clang::Diagnostic &Info) override
    {
    }

  private:
    size_t file_count_ = 0;
    size_t diagnostic_count_ = 0;
};

std::vector<std::unique_ptr<clang::ASTUnit>>
parse_project_asts(const clang::tooling::CompilationDatabase &compilation_db,
                   const std::vector<std::string> &source_files)
{
    clang::tooling::ClangTool tool(compilation_db, source_files);

    MyDiagnosticConsumer diag_consumer;
    tool.setDiagnosticConsumer(&diag_consumer);

    std::vector<std::unique_ptr<clang::ASTUnit>> ast_units;
    tool.buildASTs(ast_units);
    return ast_units;
}

} // namespace prism::ast_generation