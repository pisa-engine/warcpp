#pragma once

#include <cctype>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>

namespace warcpp {

class Warc_Record;
struct Invalid_Version {
    std::string version;
};
struct Invalid_Field {
    std::string field;
};
struct Missing_Mandatory_Fields {};
struct Incomplete_Record {};
using Warc_Error =
    std::variant<Invalid_Version, Invalid_Field, Missing_Mandatory_Fields, Incomplete_Record>;
using Result = std::variant<Warc_Record, Warc_Error>;

using Field_Map = std::unordered_map<std::string, std::string>;

class Warc_Format_Error : public std::runtime_error {
   public:
    Warc_Format_Error(std::string line, std::string message) : std::runtime_error(message + line) {}
};

bool read_warc_record(std::istream &in, Warc_Record &record);

//! A WARC record.
class Warc_Record {
   private:
    std::string version_;
    Field_Map   fields_;
    std::string content_;

    static std::string const Warc_Type;
    static std::string const Warc_Target_Uri;
    static std::string const Warc_Trec_Id;
    static std::string const Content_Length;
    static std::string const Response;

   public:
    Warc_Record() = default;
    explicit Warc_Record(std::string version) : version_(std::move(version)) {}
    [[nodiscard]] auto type() const -> std::string const & { return fields_.at(Warc_Type); }
    [[nodiscard]] auto has(std::string const &field) const noexcept -> bool {
        return fields_.find(field) != fields_.end();
    }
    [[nodiscard]] auto valid() const noexcept -> bool {
        return has(Warc_Type) && has(Content_Length);
    }
    [[nodiscard]] auto valid_response() const noexcept -> bool {
        return valid() && has(Warc_Target_Uri) && type() == Response && has(Warc_Trec_Id);
    }
    [[nodiscard]] auto content_length() const -> std::size_t {
        auto &field_value = fields_.at(Content_Length);
        try {
            return std::stoi(field_value);
        } catch (std::invalid_argument &error) {
            throw Warc_Format_Error(field_value, "could not parse content length: ");
        }
    }
    [[nodiscard]] auto content() -> std::string & { return content_; }
    [[nodiscard]] auto content() const -> std::string const & { return content_; }
    [[nodiscard]] auto url() const -> std::string const & {
        return fields_.at(Warc_Target_Uri);
    }
    [[nodiscard]] auto trecid() const -> std::string const & {
        return fields_.at(Warc_Trec_Id);
    }
    [[nodiscard]] auto field(std::string const &name) const -> std::optional<std::string> {
        if (auto pos = fields_.find(name); pos != fields_.end()) {
            return pos->second;
        }
        return std::nullopt;
    }

    friend bool read_warc_header(std::istream &in, Warc_Record &record);
    friend bool read_warc_record(std::istream &in, Warc_Record &record);
    friend auto warc_record(std::istream &in) -> Result;
    friend auto following_warc_record(std::istream &in) -> Result;
    friend std::ostream &operator<<(std::ostream &os, Warc_Record const &record);
};

std::string const Warc_Record::Warc_Type       = "warc-type";
std::string const Warc_Record::Warc_Target_Uri = "warc-target-uri";
std::string const Warc_Record::Warc_Trec_Id    = "warc-trec-id";
std::string const Warc_Record::Content_Length  = "content-length";
std::string const Warc_Record::Response        = "response";


template <typename StringRange>
[[nodiscard]] std::pair<StringRange, StringRange> split(StringRange str, char delim) {
    auto split_pos    = std::find(str.begin(), str.end(), delim);
    auto second_begin = split_pos != str.end() ? std::next(split_pos) : split_pos;
    return {{str.begin(), split_pos}, {second_begin, str.end()}};
}

template <typename StringRange>
[[nodiscard]] std::string trim(StringRange str) {
    auto begin = str.begin();
    auto end   = str.end();
    begin      = std::find_if_not(begin, end, [](char c) { return std::isspace(c); });
    end        = std::find_if(begin, end, [](char c) { return std::isspace(c); });
    return StringRange(begin, end);
}

bool read_version(std::istream &in, std::string &version) {
    std::string_view prefix = "WARC/";
    std::string line{};
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.size() < 6 or std::string_view(&line[0], prefix.size()) != prefix) {
            continue;
        }
        version = std::string(std::next(line.begin(), prefix.size()), line.end());
        return true;
    }
    return false;
}

void read_fields(std::istream &in, Field_Map &fields) {
    std::string line;
    std::getline(in, line);
    while (not line.empty() && line != "\r") {
        auto[name, value] = split(line, ':');
        if (name.empty() || value.empty()) {
            throw Warc_Format_Error(line, "could not parse field: ");
        }
        name  = trim(name);
        value = trim(value);
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
            return std::tolower(c);
        });
        fields[std::string(name.begin(), name.end())] = std::string(value.begin(), value.end());
        std::getline(in, line);
    }
}

bool read_warc_header(std::istream &in, Warc_Record &record) {
    while (not record.valid()) {
        if (not read_version(in, record.version_)) {
            return false;
        }
        read_fields(in, record.fields_);
        if (in.eof()) {
            return false;
        }
    }
    return true;
}

/**
 *
 * 1. parse version, throw otherwise
 * 2. parse header and skip one CRLF
 * 3. read `content-length` bytes to `content`
 * 4. skip two CRLF
 * 5. done
 *
 */
bool read_warc_record(std::istream &in, Warc_Record &record) {
    record.version_.clear();
    record.fields_.clear();
    record.content_.clear();
    if (not read_warc_header(in, record)) {
        return false;
    }
    if (record.content_length() > 0) {
        std::size_t length = record.content_length();
        record.content_.resize(length);
        if (not in.read(&record.content_[0], length)) {
            return false;
        }
    }
    in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    return true;
}

[[nodiscard]] auto version(std::istream &in, std::string &version)
    -> std::optional<Invalid_Version>
{
    std::string_view prefix = "WARC/";
    std::string line{};
    if (not std::getline(in, line)) {
        return Invalid_Version{std::move(line)};
    }
    auto trimmed = trim(line);
    if (trimmed.size() < 6 or std::string_view(&trimmed[0], prefix.size()) != prefix) {
        return Invalid_Version{std::move(line)};
    }
    version = std::string(std::next(trimmed.begin(), prefix.size()), trimmed.end());
    return std::nullopt;
}

[[nodiscard]] auto fields(std::istream &in, Field_Map &fields) -> std::optional<Invalid_Field> {
    std::string line;
    std::getline(in, line);
    while (not line.empty() && line != "\r") {
        auto[name, value] = split(line, ':');
        if (name.empty() || value.empty()) {
            return Invalid_Field{line};
        }
        name  = trim(name);
        value = trim(value);
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
            return std::tolower(c);
        });
        fields[std::string(name.begin(), name.end())] = std::string(value.begin(), value.end());
        std::getline(in, line);
    }
    return std::nullopt;
}

[[nodiscard]] auto warc_record(std::istream &in) -> Result {
    Warc_Record record;
    if (auto error = version(in, record.version_); error) {
        return *error;
    }
    if (auto error = fields(in, record.fields_); error) {
        return *error;
    }
    if (not record.valid()) {
        return Missing_Mandatory_Fields{};
    }
    if (record.content_length() > 0) {
        std::size_t length = record.content_length();
        record.content_.resize(length);
        if (not in.read(&record.content_[0], length)) {
            return Incomplete_Record{};
        }
    }
    in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    return record;
}

[[nodiscard]] auto following_warc_record(std::istream &in) -> Result {
    Warc_Record record;
    std::optional<Invalid_Version> error;
    while (error = version(in, record.version_)) {
        if (in.eof()) {
            return Invalid_Version{};
        }
    }
    if (auto error = fields(in, record.fields_); error) {
        return *error;
    }
    if (not record.valid()) {
        return Missing_Mandatory_Fields{};
    }
    if (record.content_length() > 0) {
        std::size_t length = record.content_length();
        record.content_.resize(length);
        if (not in.read(&record.content_[0], length)) {
            return Incomplete_Record{};
        }
    }
    in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    return record;
}

template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...)->overloaded<Ts...>;

template <typename Variant, typename... Handlers>
auto match(Variant &&value, Handlers &&... handlers) {
    return std::visit(overloaded{std::forward<Handlers>(handlers)...}, value);
}

std::ostream &operator<<(std::ostream &os, Warc_Record const &record) {
    os << "Warc_Record {";
    os << "\t" << record.version_ << "\n";
    for (auto &&[name, value] : record.fields_) {
        os << "\t" << name << ": " << value << "\n";
    }
    return os << "}";
}

std::ostream &operator<<(std::ostream &os, Warc_Error const &error) {
    match(error,
          [&](Invalid_Version const &err) { os << "Invalid_Version(" << err.version << ")"; },
          [&](Invalid_Field const &err) { os << "Invalid_Field(" << err.field << ")"; },
          [&](Missing_Mandatory_Fields const &) { os << "Missing_Mandatory_Fields"; },
          [&](Incomplete_Record const &) { os << "Incomplete_Record"; });
    return os;
}

std::ostream &operator<<(std::ostream &os, Result const &result) {
    match(result,
          [&](Warc_Record const &record) { os << record; },
          [&](Warc_Error const &error) { os << error; });
    return os;
}

} // namespace warcpp
