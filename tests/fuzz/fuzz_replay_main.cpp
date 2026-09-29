//////////////////////////////////////////////////////////////////////
//
// Filename    : fuzz_replay_main.cpp
// Description : Runs a packet-read fuzz target over saved inputs without
//               libFuzzer, so the seed corpus and every recorded crash
//               input are replayed by the ordinary test suite with the
//               ordinary toolchain.
//
//                   fuzz_replay_<target> <file-or-directory>...
//
//               A directory contributes its regular files, in name
//               order. A file named *.hex holds the input as hex digits
//               (whitespace ignored); any other file is the raw input.
//               The driver exits 0 once every input has run, 1 when an
//               argument names nothing, no input was found at all, or an
//               input is one the target would not run (StreamFuzz.h's
//               runsInput), and an input that trips the target aborts
//               it, which the test reports as a failure.
//
//////////////////////////////////////////////////////////////////////

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "StreamFuzz.h"

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv);
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size);

namespace {

int hexDigit(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

bool load(const std::filesystem::path& path, std::vector<std::uint8_t>& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return false;
    const std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    out.clear();
    if (path.extension() != ".hex") {
        out.assign(text.begin(), text.end());
        return true;
    }
    int high = -1;
    for (char c : text) {
        if (std::isspace(static_cast<unsigned char>(c)))
            continue;
        const int digit = hexDigit(c);
        if (digit < 0)
            return false;
        if (high < 0) {
            high = digit;
        } else {
            out.push_back(static_cast<std::uint8_t>(high * 16 + digit));
            high = -1;
        }
    }
    return high < 0;
}

bool collect(const std::filesystem::path& arg, std::vector<std::filesystem::path>& inputs) {
    std::error_code ec;
    if (std::filesystem::is_regular_file(arg, ec)) {
        inputs.push_back(arg);
        return true;
    }
    if (!std::filesystem::is_directory(arg, ec))
        return false;
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(arg, ec))
        if (entry.is_regular_file())
            files.push_back(entry.path());
    std::sort(files.begin(), files.end());
    inputs.insert(inputs.end(), files.begin(), files.end());
    return true;
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::filesystem::path> inputs;
    for (int i = 1; i < argc; i++) {
        if (!collect(argv[i], inputs)) {
            std::fprintf(stderr, "fuzz replay: no such file or directory: %s\n", argv[i]);
            return 1;
        }
    }
    if (inputs.empty()) {
        std::fprintf(stderr, "fuzz replay: no inputs\n");
        return 1;
    }

    LLVMFuzzerInitialize(&argc, &argv);

    std::vector<std::uint8_t> bytes;
    for (const auto& path : inputs) {
        if (!load(path, bytes)) {
            std::fprintf(stderr, "fuzz replay: cannot read %s\n", path.string().c_str());
            return 1;
        }
        if (!de::fuzz::runsInput(bytes.size())) {
            std::fprintf(stderr, "fuzz replay: %s holds %zu bytes, which the target does not run\n",
                         path.string().c_str(), bytes.size());
            return 1;
        }
        // Named before the run, so an input that aborts is identified.
        std::fprintf(stderr, "fuzz replay: %s\n", path.string().c_str());
        LLVMFuzzerTestOneInput(bytes.data(), bytes.size());
    }
    std::fprintf(stderr, "fuzz replay: %zu inputs ran\n", inputs.size());
    return 0;
}
