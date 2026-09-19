#include "cast/CastDocument.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace cast {
namespace {

constexpr std::size_t kFileHeaderSize = 16;
constexpr std::size_t kNodeHeaderSize = 24;
constexpr std::size_t kPropertyHeaderSize = 8;
constexpr std::size_t kMaxDepth = 512;

class ParseError final : public std::runtime_error {
public:
    ParseError(std::size_t offset, std::string message)
        : std::runtime_error(std::move(message)), offset(offset) {}
    std::size_t offset;
};

class Reader {
public:
    explicit Reader(std::span<const std::byte> bytes) : bytes_(bytes) {}

    [[nodiscard]] std::size_t position() const noexcept { return position_; }
    void seek(std::size_t position) {
        if (position > bytes_.size()) {
            throw ParseError(position_, "seek exceeds file size");
        }
        position_ = position;
    }

    template <typename T>
    T read() {
        require(sizeof(T));
        T value{};
        std::memcpy(&value, bytes_.data() + position_, sizeof(T));
        position_ += sizeof(T);
        return value;
    }

    std::string readString(std::size_t length) {
        require(length);
        std::string result(length, '\0');
        std::memcpy(result.data(), bytes_.data() + position_, length);
        position_ += length;
        return result;
    }

    std::string readCString(std::size_t limit) {
        const auto start = position_;
        while (position_ < limit && position_ < bytes_.size()) {
            if (bytes_[position_++] == std::byte{0}) {
                const auto length = position_ - start - 1;
                return std::string(reinterpret_cast<const char*>(bytes_.data() + start), length);
            }
        }
        throw ParseError(start, "unterminated string property");
    }

private:
    void require(std::size_t count) const {
        if (count > bytes_.size() - std::min(position_, bytes_.size())) {
            throw ParseError(position_, "unexpected end of file");
        }
    }

    std::span<const std::byte> bytes_;
    std::size_t position_{};
};

struct PropertyLayout {
    std::size_t componentSize{};
    std::uint32_t components{1};
    bool string{};
};

PropertyLayout propertyLayout(const std::string& type, std::size_t offset) {
    if (type == "b") return {1, 1, false};
    if (type == "h") return {2, 1, false};
    if (type == "i" || type == "f") return {4, 1, false};
    if (type == "l" || type == "d") return {8, 1, false};
    if (type == "s") return {0, 1, true};
    if (type == "2v") return {4, 2, false};
    if (type == "3v") return {4, 3, false};
    if (type == "4v") return {4, 4, false};
    throw ParseError(offset, "unknown property type '" + type + "'");
}

std::size_t checkedMultiply(std::size_t a, std::size_t b, std::size_t offset) {
    if (a != 0 && b > std::numeric_limits<std::size_t>::max() / a) {
        throw ParseError(offset, "property size overflow");
    }
    return a * b;
}

Property parseProperty(Reader& reader, std::size_t nodeEnd) {
    const auto propertyOffset = reader.position();
    if (nodeEnd - std::min(nodeEnd, propertyOffset) < kPropertyHeaderSize) {
        throw ParseError(propertyOffset, "property header exceeds node boundary");
    }

    std::array<char, 2> rawType{reader.read<char>(), reader.read<char>()};
    std::string type;
    type.push_back(rawType[0]);
    if (rawType[1] != '\0') type.push_back(rawType[1]);
    const auto nameLength = reader.read<std::uint16_t>();
    const auto elementCount = reader.read<std::uint32_t>();
    if (nameLength > nodeEnd - std::min(nodeEnd, reader.position())) {
        throw ParseError(propertyOffset, "property name exceeds node boundary");
    }

    Property property;
    property.type = type;
    property.name = reader.readString(nameLength);
    property.elementCount = elementCount;
    const auto layout = propertyLayout(type, propertyOffset);
    property.componentCount = layout.components;
    property.dataOffset = reader.position();

    if (layout.string) {
        if (elementCount != 1) {
            throw ParseError(propertyOffset, "string property must contain exactly one element");
        }
        property.stringValue = reader.readCString(nodeEnd);
        property.byteSize = reader.position() - property.dataOffset;
        return property;
    }

    const auto scalarCount = checkedMultiply(elementCount, layout.components, propertyOffset);
    property.byteSize = checkedMultiply(scalarCount, layout.componentSize, propertyOffset);
    if (property.byteSize > nodeEnd - std::min(nodeEnd, reader.position())) {
        throw ParseError(propertyOffset, "property data exceeds node boundary");
    }
    reader.seek(reader.position() + property.byteSize);
    return property;
}

Node parseNode(Reader& reader, std::size_t parentEnd, std::size_t depth,
               std::vector<Diagnostic>& diagnostics) {
    const auto start = reader.position();
    if (depth > kMaxDepth) throw ParseError(start, "node nesting limit exceeded");
    if (parentEnd - std::min(parentEnd, start) < kNodeHeaderSize) {
        throw ParseError(start, "node header exceeds parent boundary");
    }

    Node node;
    node.fileOffset = start;
    node.identifier = reader.read<std::uint32_t>();
    node.byteSize = reader.read<std::uint32_t>();
    node.hash = reader.read<std::uint64_t>();
    const auto propertyCount = reader.read<std::uint32_t>();
    const auto childCount = reader.read<std::uint32_t>();
    if (node.byteSize < kNodeHeaderSize) throw ParseError(start, "node is smaller than its header");
    if (node.byteSize > parentEnd - start) throw ParseError(start, "node exceeds parent boundary");
    const auto nodeEnd = start + node.byteSize;
    const auto remaining = nodeEnd - reader.position();
    if (propertyCount > remaining / kPropertyHeaderSize) {
        throw ParseError(start, "property count cannot fit inside node");
    }
    if (childCount > remaining / kNodeHeaderSize) {
        throw ParseError(start, "child count cannot fit inside node");
    }

    node.properties.reserve(propertyCount);
    for (std::uint32_t i = 0; i < propertyCount; ++i) {
        node.properties.push_back(parseProperty(reader, nodeEnd));
    }
    node.children.reserve(childCount);
    for (std::uint32_t i = 0; i < childCount; ++i) {
        node.children.push_back(parseNode(reader, nodeEnd, depth + 1, diagnostics));
    }

    if (reader.position() != nodeEnd) {
        diagnostics.push_back({Severity::Warning, reader.position(),
            "node '" + nodeTypeName(node.identifier) + "' has " +
            std::to_string(nodeEnd - reader.position()) + " unparsed trailing bytes"});
        reader.seek(nodeEnd);
    }
    return node;
}

template <typename T>
T readAt(std::span<const std::byte> bytes, std::size_t offset) {
    T value{};
    std::memcpy(&value, bytes.data() + offset, sizeof(T));
    return value;
}

void accumulate(const Node& node, Statistics& stats) {
    ++stats.nodes;
    switch (node.identifier) {
    case 0x6C646F6D: ++stats.models; break;
    case 0x6873656D: ++stats.meshes; break;
    case 0x6C656B73: ++stats.skeletons; break;
    case 0x656E6F62: ++stats.bones; break;
    case 0x6D696E61: ++stats.animations; break;
    case 0x76727563: ++stats.curves; break;
    case 0x6C74616D: ++stats.materials; break;
    default: break;
    }
    for (const auto& child : node.children) accumulate(child, stats);
}

} // namespace

const Property* Node::findProperty(std::string_view name) const {
    const auto it = std::find_if(properties.begin(), properties.end(),
        [name](const Property& property) { return property.name == name; });
    return it == properties.end() ? nullptr : &*it;
}

std::string Node::displayName() const {
    if (const auto* name = findProperty("n"); name && name->stringValue && !name->stringValue->empty()) {
        return *name->stringValue;
    }
    return nodeTypeName(identifier);
}

Document Document::load(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) {
        Document result;
        result.sourceName_ = path.string();
        result.diagnostics_.push_back({Severity::Error, 0, "unable to open file"});
        return result;
    }
    const auto end = stream.tellg();
    if (end < 0) {
        Document result;
        result.sourceName_ = path.string();
        result.diagnostics_.push_back({Severity::Error, 0, "unable to determine file size"});
        return result;
    }
    std::vector<std::byte> bytes(static_cast<std::size_t>(end));
    stream.seekg(0);
    if (!bytes.empty()) {
        stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    if (!stream && !bytes.empty()) {
        Document result;
        result.sourceName_ = path.string();
        result.diagnostics_.push_back({Severity::Error, 0, "unable to read complete file"});
        return result;
    }
    return parse(std::move(bytes), path.string());
}

Document Document::parse(std::vector<std::byte> bytes, std::string sourceName) {
    Document result;
    result.sourceName_ = std::move(sourceName);
    result.bytes_ = std::make_shared<std::vector<std::byte>>(std::move(bytes));
    try {
        if (result.bytes_->size() < kFileHeaderSize) throw ParseError(0, "file is smaller than Cast header");
        Reader reader(*result.bytes_);
        const auto magic = reader.read<std::uint32_t>();
        if (magic != kMagic) throw ParseError(0, "invalid Cast magic; expected 'cast'");
        result.version_ = reader.read<std::uint32_t>();
        const auto rootCount = reader.read<std::uint32_t>();
        result.flags_ = reader.read<std::uint32_t>();
        if (rootCount > (result.bytes_->size() - kFileHeaderSize) / kNodeHeaderSize) {
            throw ParseError(8, "root count cannot fit inside file");
        }
        if (result.version_ != 1) {
            result.diagnostics_.push_back({Severity::Warning, 4,
                "unrecognized Cast version " + std::to_string(result.version_)});
        }
        result.roots_.reserve(rootCount);
        for (std::uint32_t i = 0; i < rootCount; ++i) {
            auto root = parseNode(reader, result.bytes_->size(), 0, result.diagnostics_);
            if (root.identifier != 0x746F6F72) {
                result.diagnostics_.push_back({Severity::Warning, root.fileOffset,
                    "top-level node is not a Root node"});
            }
            result.roots_.push_back(std::move(root));
        }
        if (reader.position() != result.bytes_->size()) {
            result.diagnostics_.push_back({Severity::Warning, reader.position(),
                std::to_string(result.bytes_->size() - reader.position()) + " trailing file bytes"});
        }
        result.diagnostics_.push_back({Severity::Info, 0,
            "loaded " + std::to_string(rootCount) + " root node(s)"});
    } catch (const ParseError& error) {
        result.roots_.clear();
        result.diagnostics_.push_back({Severity::Error, error.offset, error.what()});
    } catch (const std::exception& error) {
        result.roots_.clear();
        result.diagnostics_.push_back({Severity::Error, 0, error.what()});
    }
    return result;
}

bool Document::valid() const noexcept {
    return std::none_of(diagnostics_.begin(), diagnostics_.end(),
        [](const Diagnostic& item) { return item.severity == Severity::Error; });
}

Statistics Document::statistics() const {
    Statistics result;
    for (const auto& root : roots_) accumulate(root, result);
    return result;
}

std::span<const std::byte> Document::propertyData(const Property& property) const {
    if (property.dataOffset > bytes_->size() || property.byteSize > bytes_->size() - property.dataOffset) {
        return {};
    }
    return std::span(*bytes_).subspan(property.dataOffset, property.byteSize);
}

std::vector<float> Document::floatValues(const Property& property) const {
    const auto data = propertyData(property);
    const auto count = static_cast<std::size_t>(property.elementCount) * property.componentCount;
    std::vector<float> result;
    // CAST float/vector arrays already have the destination representation.
    // Avoid a string-type dispatch and push_back for every vertex/key channel.
    if(property.type=="f"||property.type=="2v"||property.type=="3v"||property.type=="4v"){
        if(count>data.size()/sizeof(float))return {};
        result.resize(count);if(count)std::memcpy(result.data(),data.data(),count*sizeof(float));return result;
    }
    result.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        if (property.type == "f" || property.type == "2v" || property.type == "3v" || property.type == "4v")
            result.push_back(readAt<float>(data, i * 4));
        else if (property.type == "d") result.push_back(static_cast<float>(readAt<double>(data, i * 8)));
        else if (property.type == "b") result.push_back(static_cast<float>(readAt<std::uint8_t>(data, i)));
        else if (property.type == "h") result.push_back(static_cast<float>(readAt<std::uint16_t>(data, i * 2)));
        else if (property.type == "i") result.push_back(static_cast<float>(readAt<std::uint32_t>(data, i * 4)));
        else if (property.type == "l") result.push_back(static_cast<float>(readAt<std::uint64_t>(data, i * 8)));
    }
    return result;
}

std::vector<std::uint64_t> Document::unsignedValues(const Property& property) const {
    const auto data = propertyData(property);
    const auto count = static_cast<std::size_t>(property.elementCount) * property.componentCount;
    std::vector<std::uint64_t> result;
    result.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        if (property.type == "b") result.push_back(readAt<std::uint8_t>(data, i));
        else if (property.type == "h") result.push_back(readAt<std::uint16_t>(data, i * 2));
        else if (property.type == "i") result.push_back(readAt<std::uint32_t>(data, i * 4));
        else if (property.type == "l") result.push_back(readAt<std::uint64_t>(data, i * 8));
    }
    return result;
}

std::string Document::propertyPreview(const Property& property, std::size_t maxValues) const {
    if (property.stringValue) return '"' + *property.stringValue + '"';
    const auto data = propertyData(property);
    const auto total = static_cast<std::size_t>(property.elementCount) * property.componentCount;
    const auto count = std::min(total, maxValues);
    std::ostringstream out;
    out << '[';
    for (std::size_t i = 0; i < count; ++i) {
        if (i) out << ", ";
        if (property.type == "b") out << +readAt<std::uint8_t>(data, i);
        else if (property.type == "h") out << readAt<std::uint16_t>(data, i * 2);
        else if (property.type == "i") out << readAt<std::uint32_t>(data, i * 4);
        else if (property.type == "l") out << readAt<std::uint64_t>(data, i * 8);
        else if (property.type == "d") out << readAt<double>(data, i * 8);
        else out << std::setprecision(5) << readAt<float>(data, i * 4);
    }
    if (count < total) out << ", ...";
    out << ']';
    return out.str();
}

std::string fourCC(std::uint32_t identifier) {
    std::string text(4, '.');
    for (int i = 0; i < 4; ++i) {
        const auto c = static_cast<char>((identifier >> (i * 8)) & 0xFF);
        text[i] = c >= 32 && c <= 126 ? c : '.';
    }
    return text;
}

std::string nodeTypeName(std::uint32_t identifier) {
    static const std::unordered_map<std::uint32_t, const char*> names{
        {0x746F6F72, "Root"}, {0x6C646F6D, "Model"}, {0x6873656D, "Mesh"},
        {0x72696168, "Hair"}, {0x68736C62, "Blend Shape"}, {0x6C656B73, "Skeleton"},
        {0x656E6F62, "Bone"}, {0x6568646B, "IK Handle"}, {0x74736E63, "Constraint"},
        {0x6D696E61, "Animation"}, {0x76727563, "Curve"}, {0x564F4D43, "Curve Mode Override"},
        {0x6669746E, "Notification Track"}, {0x6C74616D, "Material"}, {0x656C6966, "File"},
        {0x726C6F63, "Color"}, {0x74736E69, "Instance"}, {0x6174656D, "Metadata"}
    };
    const auto it = names.find(identifier);
    return it != names.end() ? it->second : "Unknown (" + fourCC(identifier) + ")";
}

} // namespace cast
