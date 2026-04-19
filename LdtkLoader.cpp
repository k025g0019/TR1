#include "LdtkLoader.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

enum class JsonType {
    Null,
    Bool,
    Number,
    String,
    Array,
    Object,
};

struct JsonValue {
    JsonType type = JsonType::Null;
    bool boolValue = false;
    double numberValue = 0.0;
    std::string stringValue;
    std::vector<JsonValue> arrayValue;
    std::map<std::string, JsonValue> objectValue;

    const JsonValue* Find(const std::string& key) const {
        if (type != JsonType::Object) {
            return nullptr;
        }

        const auto iterator = objectValue.find(key);
        if (iterator == objectValue.end()) {
            return nullptr;
        }
        return &iterator->second;
    }
};

[[noreturn]] void ThrowJsonError(const std::string& message, std::size_t position) {
    std::ostringstream stream;
    stream << "LDtk JSON parse error at " << position << ": " << message;
    throw std::runtime_error(stream.str());
}

std::string EncodeUtf8(unsigned int codePoint) {
    std::string result;

    if (codePoint <= 0x7Fu) {
        result.push_back(static_cast<char>(codePoint));
    } else if (codePoint <= 0x7FFu) {
        result.push_back(static_cast<char>(0xC0u | ((codePoint >> 6) & 0x1Fu)));
        result.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
    } else if (codePoint <= 0xFFFFu) {
        result.push_back(static_cast<char>(0xE0u | ((codePoint >> 12) & 0x0Fu)));
        result.push_back(static_cast<char>(0x80u | ((codePoint >> 6) & 0x3Fu)));
        result.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
    } else {
        result.push_back(static_cast<char>(0xF0u | ((codePoint >> 18) & 0x07u)));
        result.push_back(static_cast<char>(0x80u | ((codePoint >> 12) & 0x3Fu)));
        result.push_back(static_cast<char>(0x80u | ((codePoint >> 6) & 0x3Fu)));
        result.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
    }

    return result;
}

int HexDigitToInt(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return 10 + (ch - 'a');
    }
    if (ch >= 'A' && ch <= 'F') {
        return 10 + (ch - 'A');
    }
    return -1;
}

class JsonParser {
public:
    explicit JsonParser(std::string text) : text_(std::move(text)) {}

    JsonValue Parse() {
        JsonValue value = ParseValue();
        SkipWhitespace();

        if (position_ != text_.size()) {
            ThrowJsonError("unexpected trailing characters", position_);
        }
        return value;
    }

private:
    JsonValue ParseValue() {
        SkipWhitespace();
        if (position_ >= text_.size()) {
            ThrowJsonError("unexpected end of file", position_);
        }

        switch (text_[position_]) {
        case '{':
            return ParseObject();
        case '[':
            return ParseArray();
        case '"':
            return ParseStringValue();
        case 't':
            return ParseTrue();
        case 'f':
            return ParseFalse();
        case 'n':
            return ParseNull();
        default:
            if (text_[position_] == '-' ||
                std::isdigit(static_cast<unsigned char>(text_[position_]))) {
                return ParseNumber();
            }
            ThrowJsonError("unexpected token", position_);
        }
    }

    JsonValue ParseObject() {
        JsonValue object;
        object.type = JsonType::Object;

        Expect('{');
        SkipWhitespace();

        if (TryConsume('}')) {
            return object;
        }

        while (true) {
            if (position_ >= text_.size() || text_[position_] != '"') {
                ThrowJsonError("object key must be a string", position_);
            }

            const std::string key = ParseString();
            SkipWhitespace();
            Expect(':');
            SkipWhitespace();
            object.objectValue[key] = ParseValue();
            SkipWhitespace();

            if (TryConsume('}')) {
                break;
            }
            Expect(',');
            SkipWhitespace();
        }

        return object;
    }

    JsonValue ParseArray() {
        JsonValue array;
        array.type = JsonType::Array;

        Expect('[');
        SkipWhitespace();

        if (TryConsume(']')) {
            return array;
        }

        while (true) {
            array.arrayValue.push_back(ParseValue());
            SkipWhitespace();

            if (TryConsume(']')) {
                break;
            }
            Expect(',');
            SkipWhitespace();
        }

        return array;
    }

    JsonValue ParseStringValue() {
        JsonValue value;
        value.type = JsonType::String;
        value.stringValue = ParseString();
        return value;
    }

    std::string ParseString() {
        Expect('"');

        std::string result;
        while (position_ < text_.size()) {
            const char ch = text_[position_++];

            if (ch == '"') {
                return result;
            }

            if (ch != '\\') {
                result.push_back(ch);
                continue;
            }

            if (position_ >= text_.size()) {
                ThrowJsonError("unterminated escape sequence", position_);
            }

            const char escaped = text_[position_++];
            switch (escaped) {
            case '"':
            case '\\':
            case '/':
                result.push_back(escaped);
                break;
            case 'b':
                result.push_back('\b');
                break;
            case 'f':
                result.push_back('\f');
                break;
            case 'n':
                result.push_back('\n');
                break;
            case 'r':
                result.push_back('\r');
                break;
            case 't':
                result.push_back('\t');
                break;
            case 'u': {
                if (position_ + 4 > text_.size()) {
                    ThrowJsonError("invalid unicode escape", position_);
                }

                unsigned int codePoint = 0;
                for (int index = 0; index < 4; ++index) {
                    const int value = HexDigitToInt(text_[position_ + index]);
                    if (value < 0) {
                        ThrowJsonError("invalid unicode escape", position_);
                    }
                    codePoint = (codePoint << 4) | static_cast<unsigned int>(value);
                }
                position_ += 4;
                result += EncodeUtf8(codePoint);
                break;
            }
            default:
                ThrowJsonError("unknown escape sequence", position_);
            }
        }

        ThrowJsonError("unterminated string", position_);
    }

    JsonValue ParseNumber() {
        const char* begin = text_.c_str() + position_;
        char* end = nullptr;
        const double value = std::strtod(begin, &end);

        if (end == begin) {
            ThrowJsonError("invalid number", position_);
        }

        position_ += static_cast<std::size_t>(end - begin);

        JsonValue number;
        number.type = JsonType::Number;
        number.numberValue = value;
        return number;
    }

    JsonValue ParseTrue() {
        ExpectWord("true");

        JsonValue value;
        value.type = JsonType::Bool;
        value.boolValue = true;
        return value;
    }

    JsonValue ParseFalse() {
        ExpectWord("false");

        JsonValue value;
        value.type = JsonType::Bool;
        value.boolValue = false;
        return value;
    }

    JsonValue ParseNull() {
        ExpectWord("null");

        JsonValue value;
        value.type = JsonType::Null;
        return value;
    }

    void SkipWhitespace() {
        while (position_ < text_.size() &&
               std::isspace(static_cast<unsigned char>(text_[position_]))) {
            ++position_;
        }
    }

    void Expect(char token) {
        if (position_ >= text_.size() || text_[position_] != token) {
            std::ostringstream stream;
            stream << "expected '" << token << "'";
            ThrowJsonError(stream.str(), position_);
        }
        ++position_;
    }

    void ExpectWord(std::string_view word) {
        if (text_.compare(position_, word.size(), word) != 0) {
            ThrowJsonError("unexpected literal", position_);
        }
        position_ += word.size();
    }

    bool TryConsume(char token) {
        if (position_ < text_.size() && text_[position_] == token) {
            ++position_;
            return true;
        }
        return false;
    }

    std::string text_;
    std::size_t position_ = 0;
};

const JsonValue& RequireField(const JsonValue& object, const char* key) {
    const JsonValue* value = object.Find(key);
    if (value == nullptr) {
        std::ostringstream stream;
        stream << "required field is missing: " << key;
        throw std::runtime_error(stream.str());
    }
    return *value;
}

const JsonValue* FindField(const JsonValue& object, const char* key) {
    return object.Find(key);
}

const std::vector<JsonValue>& AsArray(const JsonValue& value, const char* label) {
    if (value.type != JsonType::Array) {
        std::ostringstream stream;
        stream << label << " must be an array";
        throw std::runtime_error(stream.str());
    }
    return value.arrayValue;
}

const std::map<std::string, JsonValue>& AsObject(const JsonValue& value, const char* label) {
    if (value.type != JsonType::Object) {
        std::ostringstream stream;
        stream << label << " must be an object";
        throw std::runtime_error(stream.str());
    }
    return value.objectValue;
}

std::string AsString(const JsonValue& value, const char* label) {
    if (value.type != JsonType::String) {
        std::ostringstream stream;
        stream << label << " must be a string";
        throw std::runtime_error(stream.str());
    }
    return value.stringValue;
}

int AsInt(const JsonValue& value, const char* label) {
    if (value.type != JsonType::Number) {
        std::ostringstream stream;
        stream << label << " must be a number";
        throw std::runtime_error(stream.str());
    }
    return static_cast<int>(std::lround(value.numberValue));
}

std::string ReadTextFile(const std::filesystem::path& filePath) {
    std::ifstream input(filePath, std::ios::binary);
    if (!input) {
        std::ostringstream stream;
        stream << "LDtk file could not be opened: " << filePath.u8string();
        throw std::runtime_error(stream.str());
    }

    std::string text(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());

    if (text.size() >= 3 &&
        static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF) {
        text.erase(0, 3);
    }

    return text;
}

std::vector<int> ParseIntGridCsv(const JsonValue& layerValue) {
    std::vector<int> values;

    const JsonValue* csvValue = FindField(layerValue, "intGridCsv");
    if (csvValue == nullptr || csvValue->type == JsonType::Null) {
        return values;
    }

    const auto& csvArray = AsArray(*csvValue, "intGridCsv");
    values.reserve(csvArray.size());
    for (const JsonValue& entry : csvArray) {
        values.push_back(AsInt(entry, "intGridCsv entry"));
    }

    return values;
}

std::vector<LdtkEntityData> ParseEntities(const JsonValue& layerValue, int gridSize) {
    std::vector<LdtkEntityData> entities;

    const JsonValue* entitiesValue = FindField(layerValue, "entityInstances");
    if (entitiesValue == nullptr || entitiesValue->type == JsonType::Null) {
        return entities;
    }

    const auto& entityArray = AsArray(*entitiesValue, "entityInstances");
    entities.reserve(entityArray.size());

    for (const JsonValue& entityValue : entityArray) {
        AsObject(entityValue, "entity");

        LdtkEntityData entity;
        entity.identifier =
            AsString(RequireField(entityValue, "__identifier"), "__identifier");

        if (const JsonValue* gridValue = FindField(entityValue, "__grid");
            gridValue != nullptr && gridValue->type == JsonType::Array &&
            gridValue->arrayValue.size() >= 2) {
            entity.gridX = AsInt(gridValue->arrayValue[0], "__grid[0]");
            entity.gridY = AsInt(gridValue->arrayValue[1], "__grid[1]");
        } else {
            const JsonValue& pxValue = RequireField(entityValue, "px");
            const auto& pxArray = AsArray(pxValue, "px");
            if (pxArray.size() < 2) {
                throw std::runtime_error("px must contain at least 2 elements");
            }

            entity.gridX = AsInt(pxArray[0], "px[0]") / gridSize;
            entity.gridY = AsInt(pxArray[1], "px[1]") / gridSize;
        }

        entities.push_back(entity);
    }

    return entities;
}

LdtkLevelData ParseLevel(const JsonValue& levelValue) {
    AsObject(levelValue, "level");

    LdtkLevelData level;
    level.identifier = AsString(RequireField(levelValue, "identifier"), "identifier");

    const JsonValue* layersValue = FindField(levelValue, "layerInstances");
    if (layersValue == nullptr || layersValue->type == JsonType::Null) {
        throw std::runtime_error("external level files are not supported in this demo");
    }

    const auto& layers = AsArray(*layersValue, "layerInstances");
    bool hasCollision = false;

    for (const JsonValue& layerValue : layers) {
        AsObject(layerValue, "layer");

        const std::string layerType =
            AsString(RequireField(layerValue, "__type"), "__type");
        const std::string layerIdentifier =
            AsString(RequireField(layerValue, "__identifier"), "__identifier");

        if (layerType == "IntGrid") {
            const std::vector<int> gridValues = ParseIntGridCsv(layerValue);
            if (gridValues.empty()) {
                continue;
            }

            if (hasCollision && layerIdentifier != "Collision") {
                continue;
            }

            level.gridWidth = AsInt(RequireField(layerValue, "__cWid"), "__cWid");
            level.gridHeight = AsInt(RequireField(layerValue, "__cHei"), "__cHei");
            level.intGridCsv = gridValues;
            hasCollision = true;
        } else if (layerType == "Entities") {
            const int gridSize =
                AsInt(RequireField(layerValue, "__gridSize"), "__gridSize");
            level.entities = ParseEntities(layerValue, gridSize);
        }
    }

    if (!hasCollision) {
        throw std::runtime_error("IntGrid layer was not found");
    }

    if (level.gridWidth <= 0 || level.gridHeight <= 0) {
        throw std::runtime_error("invalid LDtk grid size");
    }

    if (level.intGridCsv.size() !=
        static_cast<std::size_t>(level.gridWidth * level.gridHeight)) {
        throw std::runtime_error("intGridCsv size does not match the level size");
    }

    return level;
}

}  // namespace

LdtkProjectData LoadLdtkProject(const std::filesystem::path& filePath) {
    const std::string text = ReadTextFile(filePath);
    JsonParser parser(text);
    const JsonValue root = parser.Parse();

    AsObject(root, "root");

    const auto& levelArray = AsArray(RequireField(root, "levels"), "levels");

    LdtkProjectData project;
    project.levels.reserve(levelArray.size());
    for (const JsonValue& levelValue : levelArray) {
        project.levels.push_back(ParseLevel(levelValue));
    }

    if (project.levels.empty()) {
        throw std::runtime_error("the LDtk file does not contain any levels");
    }

    return project;
}
