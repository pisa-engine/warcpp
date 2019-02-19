#pragma once

#include <cctype>
#include <iostream>
#include <optional>
#include <regex>
#include <string>
#include <unordered_map>

namespace warcpp {

using Field_Map = std::unordered_map<std::string, std::string>;

class Warc_Format_Error : public std::runtime_error {
   public:
    Warc_Format_Error(std::string line, std::string message) : std::runtime_error(message + line) {}
};

class Warc_Record;
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
    explicit Warc_Record(std::string version)
        : version_(std::move(version)){}[[nodiscard]] auto type() const -> std::string const & {
        return fields_.at(Warc_Type);
    }
    [[nodiscard]] auto has(std::string const &field) const noexcept -> bool {
        return fields_.find(field) != fields_.end();
    }
    [[nodiscard]] auto valid() const noexcept -> bool {
        return has(Warc_Type) && has(Warc_Target_Uri) && has(Warc_Trec_Id) && has(Content_Length) &&
               type() == Response;
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
    std::regex version_pattern("^WARC/(.+)$");
    std::smatch sm;
    std::string line{};
    while (std::getline(in, line)) {
        line = trim(line);
        if (not std::regex_search(line, sm, version_pattern)) {
            continue;
        }
        version = sm.str(1);
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

} // namespace warcpp
