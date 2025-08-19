#include "errors.h"
#include <format>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/std.h>

namespace candlewick {
RAIIException::RAIIException(std::string_view msg,
                             std::source_location location)
    : std::runtime_error(
          std::format("{:s}({:d}) RAIIException: SDL error \'{}\'",
                      location.file_name(), location.line(), msg.data())) {}

namespace detail {
  std::string _error_message_impl(std::string_view fname,
                                  std::string_view fmtstr,
                                  std::format_args args) {
    return std::format("{:s} :: {:s}", fname, std::vformat(fmtstr, args));
  }
} // namespace detail

void unreachable_with_message(std::string_view msg,
                              std::source_location location) {
  spdlog::error("{} :: {:s}", location, msg);
  ::candlewick::unreachable();
}
} // namespace candlewick
