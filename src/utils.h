#include <expected>
#include <filesystem>
#include <string>

namespace utils
{
std::expected<std::string, std::string>
read_file(const std::filesystem::path &path);

class ScopedDisable
{
  public:
    ScopedDisable(bool condition);
    ~ScopedDisable();

  private:
    bool condition_;
};
} // namespace utils