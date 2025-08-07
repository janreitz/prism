#include <expected>
#include <filesystem>
#include <string>

namespace utils
{
std::expected<std::string, std::string>
read_file(const std::filesystem::path &path);
}