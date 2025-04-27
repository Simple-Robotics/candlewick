#include <nng/nng.h>
#include <magic_enum/magic_enum.hpp>

const char *str_from_nng_erro(int val) {
  return magic_enum::enum_name(nng_errno_enum(val)).data();
}
