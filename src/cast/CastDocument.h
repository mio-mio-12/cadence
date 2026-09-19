#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace cast {

enum class Severity { Info, Warning, Error };

struct Diagnostic {
    Severity severity{};
    std::size_t offset{};
    std::string message;
};

struct Property {
    std::string type;
    std::string name;
    std::uint32_t elementCount{};
    std::uint32_t componentCount{1};
    std::size_t dataOffset{};
    std::size_t byteSize{};
    std::optional<std::string> stringValue;
};

struct Node {
    std::uint32_t identifier{};
    std::uint32_t byteSize{};
    std::uint64_t hash{};
    std::size_t fileOffset{};
    std::vector<Property> properties;
    std::vector<Node> children;

    [[nodiscard]] const Property* findProperty(std::string_view name) const;
    [[nodiscard]] std::string displayName() const;
};

struct Statistics {
    std::size_t nodes{};
    std::size_t models{};
    std::size_t meshes{};
    std::size_t skeletons{};
    std::size_t bones{};
    std::size_t animations{};
    std::size_t curves{};
    std::size_t materials{};
};

class Document {
public:
    static constexpr std::uint32_t kMagic = 0x74736163;

    [[nodiscard]] static Document load(const std::filesystem::path& path);
    [[nodiscard]] static Document parse(std::vector<std::byte> bytes,
                                        std::string sourceName = "memory");

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] std::uint32_t version() const noexcept { return version_; }
    [[nodiscard]] std::uint32_t flags() const noexcept { return flags_; }
    [[nodiscard]] const std::string& sourceName() const noexcept { return sourceName_; }
    [[nodiscard]] std::size_t fileSize() const noexcept { return bytes_->size(); }
    [[nodiscard]] const std::vector<Node>& roots() const noexcept { return roots_; }
    [[nodiscard]] const std::vector<Diagnostic>& diagnostics() const noexcept { return diagnostics_; }
    [[nodiscard]] Statistics statistics() const;
    [[nodiscard]] std::span<const std::byte> propertyData(const Property& property) const;
    [[nodiscard]] std::vector<float> floatValues(const Property& property) const;
    [[nodiscard]] std::vector<std::uint64_t> unsignedValues(const Property& property) const;
    [[nodiscard]] std::string propertyPreview(const Property& property,
                                              std::size_t maxValues = 6) const;

private:
    std::string sourceName_;
    std::shared_ptr<std::vector<std::byte>> bytes_ =
        std::make_shared<std::vector<std::byte>>();
    std::uint32_t version_{};
    std::uint32_t flags_{};
    std::vector<Node> roots_;
    std::vector<Diagnostic> diagnostics_;
};

[[nodiscard]] std::string nodeTypeName(std::uint32_t identifier);
[[nodiscard]] std::string fourCC(std::uint32_t identifier);

} // namespace cast
