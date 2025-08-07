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

class ProgressConsumer : public clang::DiagnosticConsumer
{
  public:
    ProgressConsumer(std::optional<ProgressCallback> progress_callback,
                     std::optional<ErrorCallback> error_callback,
                     size_t total_file_count)
        : progress_callback_(progress_callback),
          error_callback_(error_callback_), total_file_count_(total_file_count)
    {
    }
    //   Callback to inform the diagnostic client that processing of a
    //   source file is beginning.
    void BeginSourceFile(const clang::LangOptions &LangOpts,
                         const clang::Preprocessor *PP = nullptr) override
    {

        std::cerr << "Starting to process file number " << processed_file_count_
                  << std::endl;

        if (PP) {
            auto &SM = PP->getSourceManager();
            auto FID = SM.getMainFileID();
            if (FID.isValid()) {
                if (auto *Entry = SM.getFileEntryForID(FID)) {
                    std::cerr << "Processing file: " << Entry->getName().data()
                              << std::endl;
                    current_file_ = Entry->getName();
                }
            } else {
                std::cerr << "MainFileID invalid" << std::endl;
                current_file_ = "unknown";
            }
        } else {
            std::cerr << std::source_location().function_name()
                      << "(no preprocessor)" << std::endl;
            current_file_ = "unknown";
        }
        if (progress_callback_.has_value()) {
            progress_callback_.value()(processed_file_count_, total_file_count_,
                                       current_file_);
        }

        processed_file_count_++;
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
    size_t processed_file_count_ = 0;
    size_t total_file_count_ = 0;
    size_t diagnostic_count_ = 0;
    std::string current_file_;
    std::optional<ProgressCallback> progress_callback_;
    std::optional<ErrorCallback> error_callback_;
};

std::vector<std::unique_ptr<clang::ASTUnit>>
parse_project_asts(const clang::tooling::CompilationDatabase &compilation_db,
                   const std::vector<std::string> &source_files,
                   std::optional<ProgressCallback> progress_callback,
                   std::optional<ErrorCallback> error_callback)
{
    clang::tooling::ClangTool tool(compilation_db, source_files);

    ProgressConsumer diag_consumer(progress_callback, error_callback,
                                   source_files.size());
    tool.setDiagnosticConsumer(&diag_consumer);

    std::vector<std::unique_ptr<clang::ASTUnit>> ast_units;
    const int result_code = tool.buildASTs(ast_units);

    std::cerr << std::source_location().function_name() << ": ";
    if (result_code == 0) {
        std::cerr << "Parsing success, parsed " << ast_units.size()
                  << "Translation units" << std::endl;
    } else if (result_code == 1) {
        std::cerr << "Any error occurred" << std::endl;
    } else if (result_code == 2) {
        std::cerr << "There is no error but some files are skipped due to "
                     "missing compile commands."
                  << std::endl;
    }
    return ast_units;
}

} // namespace prism::ast_generation