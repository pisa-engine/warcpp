#pragma once

#include <cctype>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>

namespace warcpp {

struct Invalid_Version { std::string version; };
struct Invalid_Field { std::string field; };
struct Missing_Mandatory_Fields {};
struct Incomplete_Record {};
using Error = std::variant<Invalid_Version,
                           Invalid_Field,
                           Missing_Mandatory_Fields,
                           Incomplete_Record>;
class Record;
using Result = std::variant<Record, Error>;

namespace detail {

    using Field_Map = std::unordered_map<std::string, std::string>;

    template <class... Ts>
    struct overloaded : Ts... {
        using Ts::operator()...;
    };

    template <class... Ts>
    overloaded(Ts...)->overloaded<Ts...>;

    template <typename StringRange>
    [[nodiscard]] std::pair<StringRange, StringRange> split(StringRange str, char delim)
    {
        auto split_pos = std::find(str.begin(), str.end(), delim);
        auto second_begin = split_pos != str.end() ? std::next(split_pos) : split_pos;
        return {{str.begin(), split_pos}, {second_begin, str.end()}};
    }

    template <typename StringRange>
    [[nodiscard]] std::string trim(StringRange str)
    {
        auto begin = str.begin();
        auto end = str.end();
        begin = std::find_if_not(begin, end, [](char c) { return std::isspace(c); });
        end = std::find_if(begin, end, [](char c) { return std::isspace(c); });
        return StringRange(begin, end);
    }

    [[nodiscard]] auto read_fields(std::istream &in, Field_Map &fields)
        -> std::optional<Invalid_Field>
    {
        std::string line;
        std::getline(in, line);
        while (not line.empty() && line != "\r") {
            auto [name, value] = split(line, ':');
            if (name.empty() || value.empty()) {
                return Invalid_Field{line};
            }
            name = trim(name);
            value = trim(value);
            std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
                return std::tolower(c);
            });
            fields[std::string(name.begin(), name.end())] = std::string(value.begin(), value.end());
            std::getline(in, line);
        }
        return std::nullopt;
    }

    [[nodiscard]] auto read_version(std::istream &in, std::string &version)
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

}; // namespace detail

class Record {
   private:
    std::string version_;
    detail::Field_Map fields_;
    std::string content_;

    static std::string const Warc_Type;
    static std::string const Warc_Target_Uri;
    static std::string const Warc_Trec_Id;
    static std::string const Content_Length;
    static std::string const Response;

   public:
    Record() = default;
    explicit Record(std::string version) : version_(std::move(version)) {}
    [[nodiscard]] auto type() const -> std::string const & { return fields_.at(Warc_Type); }
    [[nodiscard]] auto has(std::string const &field) const noexcept -> bool
    {
        return fields_.find(field) != fields_.end();
    }
    [[nodiscard]] auto valid() const noexcept -> bool
    {
        return has(Warc_Type) && has(Content_Length);
    }
    [[nodiscard]] auto valid_response() const noexcept -> bool
    {
        return valid() && has(Warc_Target_Uri) && type() == Response && has(Warc_Trec_Id);
    }
    [[nodiscard]] auto content_length() const -> std::size_t
    {
        auto &field_value = fields_.at(Content_Length);
        try {
            return std::stoi(field_value);
        } catch (std::invalid_argument &error) {
            std::ostringstream os;
            os << "could not parse content length: " << field_value;
            throw std::runtime_error(os.str());
        }
    }
    [[nodiscard]] auto content() -> std::string & { return content_; }
    [[nodiscard]] auto content() const -> std::string const & { return content_; }
    [[nodiscard]] auto url() const -> std::string const & { return fields_.at(Warc_Target_Uri); }
    [[nodiscard]] auto trecid() const -> std::string const & { return fields_.at(Warc_Trec_Id); }
    [[nodiscard]] auto field(std::string const &name) const -> std::optional<std::string>
    {
        if (auto pos = fields_.find(name); pos != fields_.end()) {
            return pos->second;
        }
        return std::nullopt;
    }

    friend auto read_record(std::istream &in) -> Result;
    friend auto read_subsequent_record(std::istream &in) -> Result;
    friend std::ostream &operator<<(std::ostream &os, Record const &record);
};

template <typename Record_Handler, typename Error_Handler>
auto match(Result const &result, Record_Handler &&record_handler, Error_Handler &&error_handler)
{
    if (auto *record = std::get_if<Record>(&result); record != nullptr) {
        if constexpr (std::is_same_v<decltype(record_handler(*record)), void>) {
            record_handler(*record);
        } else {
            return record_handler(*record);
        }
    } else {
        auto *error = std::get_if<Error>(&result);
        if constexpr (std::is_same_v<decltype(error_handler(*error)), void>) {
            error_handler(*error);
        } else {
            return error_handler(*error);
        }
    }
}

template <typename IV_Handler, typename IF_Handler, typename MMF_Handler, typename IR_Handler>
auto match(Error const &error,
           IV_Handler &&iv_handler,
           IF_Handler &&if_handler,
           MMF_Handler &&mmf_handler,
           IR_Handler &&ir_handler)
{
    if (auto *iv = std::get_if<Invalid_Version>(&error); iv != nullptr) {
        if constexpr (std::is_same_v<decltype(iv_handler(*iv)), void>) {
            iv_handler(*iv);
        } else {
            return iv_handler(*iv);
        }
    } else if (auto *ife = std::get_if<Invalid_Field>(&error); ife != nullptr) {
        if constexpr (std::is_same_v<decltype(if_handler(*ife)), void>) {
            if_handler(*ife);
        } else {
            return if_handler(*ife);
        }
    } else if (auto *mmf = std::get_if<Missing_Mandatory_Fields>(&error); mmf != nullptr) {
        if constexpr (std::is_same_v<decltype(mmf_handler(*mmf)), void>) {
            mmf_handler(*mmf);
        } else {
            return mmf_handler(*mmf);
        }
    } else if (auto *ir = std::get_if<Incomplete_Record>(&error); ir != nullptr) {
        if constexpr (std::is_same_v<decltype(ir_handler(*ir)), void>) {
            ir_handler(*ir);
        } else {
            return ir_handler(*ir);
        }
    }
}

constexpr bool holds_record(Result const &result) { return std::holds_alternative<Record>(result); }

std::string const Record::Warc_Type = "warc-type";
std::string const Record::Warc_Target_Uri = "warc-target-uri";
std::string const Record::Warc_Trec_Id = "warc-trec-id";
std::string const Record::Content_Length = "content-length";
std::string const Record::Response = "response";

/**
 *
 * 1. parse version, throw otherwise
 * 2. parse header and skip one CRLF
 * 3. read `content-length` bytes to `content`
 * 4. skip two CRLF
 * 5. done
 *
 */
[[nodiscard]] auto read_record(std::istream &in) -> Result
{
    Record record;
    if (auto error = detail::read_version(in, record.version_); error) {
        return Result(*error);
    }
    if (auto error = detail::read_fields(in, record.fields_); error) {
        return Result(*error);
    }
    if (not record.valid()) {
        return Result(Missing_Mandatory_Fields{});
    }
    if (record.content_length() > 0) {
        std::size_t length = record.content_length();
        record.content_.resize(length);
        if (not in.read(&record.content_[0], length)) {
            return Result(Incomplete_Record{});
        }
    }
    while (std::isspace(in.peek())) { in.ignore(1); }
    return Result(record);
}

[[nodiscard]] auto read_subsequent_record(std::istream &in) -> Result
{
    Record record;
    std::optional<Invalid_Version> error;
    while ((error = detail::read_version(in, record.version_))) {
        if (in.eof()) {
            return Result(Invalid_Version{});
        }
    }
    if (auto error = detail::read_fields(in, record.fields_); error) {
        return Result(*error);
    }
    if (not record.valid()) {
        return Result(Missing_Mandatory_Fields{});
    }
    if (record.content_length() > 0) {
        std::size_t length = record.content_length();
        record.content_.resize(length);
        if (not in.read(&record.content_[0], length)) {
            return Result(Incomplete_Record{});
        }
    }
    while (std::isspace(in.peek())) { in.ignore(1); }
    return Result(record);
}

std::ostream &operator<<(std::ostream &os, Record const &record)
{
    os << "Record {";
    os << "\t" << record.version_ << "\n";
    for (auto &&[name, value] : record.fields_) {
        os << "\t" << name << ": " << value << "\n";
    }
    return os << "}";
}

std::ostream &operator<<(std::ostream &os, Error const &error)
{
    match(error,
          [&](Invalid_Version const &err) { os << "Invalid_Version(" << err.version << ")"; },
          [&](Invalid_Field const &err) { os << "Invalid_Field(" << err.field << ")"; },
          [&](Missing_Mandatory_Fields const &) { os << "Missing_Mandatory_Fields"; },
          [&](Incomplete_Record const &) { os << "Incomplete_Record"; });
    return os;
}

std::ostream &operator<<(std::ostream &os, Result const &result)
{
    match(result,
          [&](Record const &record) { os << record; },
          [&](Error const &error) { os << error; });
    return os;
}

} // namespace warcpp
