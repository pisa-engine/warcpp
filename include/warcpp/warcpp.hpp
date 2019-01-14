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
void read_warc_record(std::istream &in, Warc_Record &record);

//! A WARC record.
class Warc_Record {
   private:
    std::string version_;
    Field_Map   warc_fields_;
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
        return warc_fields_.at(Warc_Type);
    }
    [[nodiscard]] auto has(std::string const &field) const noexcept -> bool {
        return warc_fields_.find(field) != warc_fields_.end();
    }
    [[nodiscard]] auto valid() const noexcept -> bool {
        return has(Warc_Type) && has(Warc_Target_Uri) && has(Warc_Trec_Id) && has(Content_Length) &&
               type() == Response;
    }
    [[nodiscard]] auto warc_content_length() const -> std::size_t {
        auto &field_value = warc_fields_.at(Content_Length);
        try {
            return std::stoi(field_value);
        } catch (std::invalid_argument &error) {
            throw Warc_Format_Error(field_value, "could not parse content length: ");
        }
    }
    [[nodiscard]] auto content() -> std::string & { return content_; }
    [[nodiscard]] auto content() const -> std::string const & { return content_; }
    [[nodiscard]] auto url() const -> std::string const & {
        return warc_fields_.at(Warc_Target_Uri);
    }
    [[nodiscard]] auto trecid() const -> std::string const & {
        return warc_fields_.at(Warc_Trec_Id);
    }
    [[nodiscard]] auto warc_field(std::string const &name) const -> std::optional<std::string> {
        if (auto pos = warc_fields_.find(name); pos != warc_fields_.end()) {
            return pos->second;
        }
        return std::nullopt;
    }

    friend void read_warc_record(std::istream &in, Warc_Record &record);
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

std::istream &read_version(std::istream &in, std::string &version) {
    std::string line{};
    while (line.empty()) {
        if (not std::getline(in, line)) {
            return in;
        }
    }
    std::regex  version_pattern("^WARC/(.+)$");
    std::smatch sm;
    line = trim(line);
    if (not std::regex_search(line, sm, version_pattern)) {
        throw Warc_Format_Error(line, "could not parse version: ");
    }
    version = sm.str(1);
    return in;
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
/**
 *
 * 1. parse version, throw otherwise
 * 2. parse header and skip one CRLF
 * 3. read `content-length` bytes to `content`
 * 4. skip two CRLF
 * 5. done
 *
 */

void read_warc_record(std::istream &in, Warc_Record &record) {
    read_version(in, record.version_);
    if (not in.eof()){
        read_fields(in, record.warc_fields_);
        if (record.warc_content_length() > 0) {
            std::size_t length = record.warc_content_length();
            record.content_.resize(length);
            in.read(&record.content_[0], length);
        }
        in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
}

} // namespace warcpp
