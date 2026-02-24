#include "errors.h"
#include <spdlog/spdlog.h>
#include <spdlog/fmt/std.h>

namespace candlewick {
RAIIException::RAIIException(std::string_view msg,
                             std::source_location location)
    : std::runtime_error(
          fmt::format("{:s}({:d}) RAIIException: SDL error \'{}\'",
                      location.file_name(), location.line(), msg.data())) {}

namespace detail {
  std::string _error_message_impl(std::string_view fname,
                                  std::string_view fmtstr,
                                  fmt::format_args args) {
    return fmt::format("{:s} :: {:s}", fname, fmt::vformat(fmtstr, args));
  }
} // namespace detail

void unreachable_with_message(std::string_view msg,
                              std::source_location location) {
  spdlog::error("{} :: {:s}", location, msg);
  ::candlewick::unreachable();
}
} // namespace candlewick
