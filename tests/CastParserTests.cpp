#include "cast/CastDocument.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string_view>
#include <vector>

namespace {

template <typename T>
void append(std::vector<std::byte>& bytes, T value) {
    const auto start = bytes.size();
    bytes.resize(start + sizeof(T));
    std::memcpy(bytes.data() + start, &value, sizeof(T));
}

void appendText(std::vector<std::byte>& bytes, std::string_view text) {
    const auto start = bytes.size();
    bytes.resize(start + text.size());
    std::memcpy(bytes.data() + start, text.data(), text.size());
}

std::vector<std::byte> minimalModel() {
    std::vector<std::byte> bytes;
    append(bytes, cast::Document::kMagic);
    append<std::uint32_t>(bytes, 1);
    append<std::uint32_t>(bytes, 1);
    append<std::uint32_t>(bytes, 0);

    // Root: header + one Model child.
    append<std::uint32_t>(bytes, 0x746F6F72);
    append<std::uint32_t>(bytes, 24 + 24 + 8 + 1 + 5);
    append<std::uint64_t>(bytes, 1);
    append<std::uint32_t>(bytes, 0);
    append<std::uint32_t>(bytes, 1);

    append<std::uint32_t>(bytes, 0x6C646F6D);
    append<std::uint32_t>(bytes, 24 + 8 + 1 + 5);
    append<std::uint64_t>(bytes, 2);
    append<std::uint32_t>(bytes, 1);
    append<std::uint32_t>(bytes, 0);
    appendText(bytes, std::string_view("s\0", 2));
    append<std::uint16_t>(bytes, 1);
    append<std::uint32_t>(bytes, 1);
    appendText(bytes, "n");
    appendText(bytes, std::string_view("test\0", 5));
    return bytes;
}

bool expect(bool condition, const char* message) {
    if (!condition) std::cerr << "FAILED: " << message << '\n';
    return condition;
}

} // namespace

int main() {
    int failures = 0;
    auto document = cast::Document::parse(minimalModel(), "minimal.cast");
    failures += !expect(document.valid(), "minimal document should parse");
    failures += !expect(document.roots().size() == 1, "one root expected");
    failures += !expect(document.statistics().models == 1, "one model expected");
    if (document.valid() && !document.roots().empty() && !document.roots()[0].children.empty()) {
        failures += !expect(document.roots()[0].children[0].displayName() == "test", "model name should decode");
    }

    auto badMagic = minimalModel();
    badMagic[0] = std::byte{'x'};
    failures += !expect(!cast::Document::parse(std::move(badMagic)).valid(), "bad magic should fail");

    auto truncated = minimalModel();
    truncated.resize(25);
    failures += !expect(!cast::Document::parse(std::move(truncated)).valid(), "truncated node should fail");

    if (failures == 0) std::cout << "All Cast parser tests passed.\n";
    return failures == 0 ? 0 : 1;
}
