#include "marketreplay/itch.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

namespace {
void usage() {
    std::cerr << "usage: marketreplay INPUT [--json] [--strict-time] [--check-every N]\n";
}
}

int main(int argc, char** argv) {
    if (argc < 2) {
        usage();
        return 2;
    }
    std::string input_path;
    bool json = false;
    marketreplay::ReplayConfig config;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--json") {
            json = true;
        } else if (argument == "--strict-time") {
            config.strict_time = true;
        } else if (argument == "--check-every") {
            if (i + 1 >= argc) {
                usage();
                return 2;
            }
            try {
                config.check_every = std::stoull(argv[++i]);
            } catch (...) {
                std::cerr << "error: --check-every requires a nonnegative integer\n";
                return 2;
            }
        } else if (!argument.empty() && argument[0] == '-') {
            std::cerr << "error: unknown option: " << argument << '\n';
            return 2;
        } else if (input_path.empty()) {
            input_path = argument;
        } else {
            usage();
            return 2;
        }
    }
    if (input_path.empty()) {
        usage();
        return 2;
    }

    std::ifstream input(input_path, std::ios::binary);
    if (!input) {
        std::cerr << "error: cannot open input: " << input_path << '\n';
        return 2;
    }

    try {
        marketreplay::ReplayEngine engine(config);
        engine.replay(input);
        if (json) {
            std::cout << engine.canonical_json() << '\n';
        } else {
            std::cout << "frames=" << engine.total_frames()
                      << " active_orders=" << engine.active_orders()
                      << " fingerprint=" << engine.state_fingerprint() << '\n';
        }
        return 0;
    } catch (const marketreplay::ReplayError& error) {
        std::cerr << "replay error: " << error.what() << '\n';
        return 3;
    } catch (const std::exception& error) {
        std::cerr << "internal error: " << error.what() << '\n';
        return 4;
    }
}
