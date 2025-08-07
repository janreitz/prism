
#include "utils.h"

#include <fstream>
#include <sstream>

namespace utils
{

std::expected<std::string, std::string>
read_file(const std::filesystem::path &path)
{
    std::ifstream file(path);
    if (!file) {
        return std::unexpected("Cannot open file: " + path.string());
    } else {
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
}
} // namespace utils