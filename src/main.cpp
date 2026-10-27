#include "ccsim/cli.hpp"

#include <exception>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    try {
        std::vector<std::string> args;
        for (int i = 1; i < argc; ++i) {
            args.emplace_back(argv[i]);
        }
        return static_cast<int>(ccsim::run(args, std::cout, std::cerr));
    } catch (const std::exception& e) {
        std::cerr << "ccsim: fatal error: " << e.what() << '\n';
    } catch (...) {
        std::cerr << "ccsim: fatal error: unknown exception\n";
    }
    return static_cast<int>(ccsim::ExitCode::RuntimeError);
}
