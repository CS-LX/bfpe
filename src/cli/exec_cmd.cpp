#include "exec_cmd.hpp"

#include "bf_interp.hpp"
#include "paths.hpp"
#include "process.hpp"

namespace bfpe {

int cmd_exec(const std::filesystem::path& root,
             const std::filesystem::path& bf_path,
             const std::vector<std::string>& run_args) {
    (void)root;
    if (!std::filesystem::exists(bf_path)) {
        print_error("error: " + path_to_utf8(bf_path) + " not found");
        return 1;
    }

    try {
        return codegen::exec_bf(std::filesystem::absolute(bf_path), run_args);
    } catch (const std::exception& exc) {
        print_error(std::string("error: ") + exc.what());
        return 1;
    }
}

}  // namespace bfpe
