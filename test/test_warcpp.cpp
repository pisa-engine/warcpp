#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"

#include <string>

#include "warcpp/warcpp.hpp"

using namespace warcpp;
using namespace warcpp::detail;

TEST_CASE("Parse WARC version", "[warc][unit]")
{
    std::string input = GENERATE(as<std::string>(),
                                 "WARC/0.18",
                                 "WARC/0.18\nUnrelated text",
                                 "\n\n\nWARC/0.18\nUnrelated text");
    GIVEN("Input: " << input)
    {
        std::istringstream in("WARC/0.18\nUnrelated text");
        std::string version;
        REQUIRE(read_version(in, version) == std::nullopt);
        REQUIRE(version == "0.18");
    }
}

TEST_CASE("Parse invalid WARC version string", "[warc][unit]")
{
    std::istringstream in("INVALID_STRING");
    std::string version;
    REQUIRE(read_version(in, version)->version == "INVALID_STRING");
}

TEST_CASE("Look for version until EOF", "[warc][unit]")
{
    std::istringstream in("");
    std::string version = "initial";
    REQUIRE(read_version(in, version)->version == "");
    REQUIRE(version == "initial");
}

TEST_CASE("Parse valid fields", "[warc][unit]")
{
    std::string input = GENERATE(as<std::string>(),
                                 "WARC-Type: warcinfo\n"
                                 "Content-Type  : application/warc-fields\n"
                                 "Content-Length: 9    \n"
                                 "\n"
                                 "REMAINDER",
                                 "WARC-Type: warcinfo\n"
                                 "Content-Type  : application/warc-fields\n"
                                 "Content-Length: 9    \r\n"
                                 "\r\n"
                                 "REMAINDER");
    GIVEN("A valid list of fields") {
        std::istringstream in(input);
        WHEN("Parse fields") {
            Field_Map fields;
            REQUIRE(read_fields(in, fields) == std::nullopt);
            THEN("Read fields are lowercase and stripped") {
                CHECK(fields["warc-type"] == "warcinfo");
                CHECK(fields["content-type"] == "application/warc-fields");
                CHECK(fields["content-length"] == "9");
            }
            THEN("Two trailing newlines are skipped") {
                std::ostringstream os;
                os << in.rdbuf();
                REQUIRE(os.str() == "REMAINDER");
            }
        }
    }
}

TEST_CASE("Parse invalid fields", "[warc][unit]")
{
    std::string input = GENERATE(as<std::string>(), "invalidfield\n", "invalid:\n", ":value\n");
    GIVEN("Field input: '" << input << "'") {
        std::istringstream in(input);
        Field_Map          fields;
        REQUIRE(read_fields(in, fields)->field ==
                std::string(input.begin(), std::prev(input.end())));
    }
}

std::string warcinfo() {
    return "WARC/0.18\n"
           "WARC-Type: warcinfo\n"
           "WARC-Date: 2009-03-65T08:43:19-0800\n"
           "WARC-Record-ID: <urn:uuid:993d3969-9643-4934-b1c6-68d4dbe55b83>\n"
           "Content-Type: application/warc-fields\n"
           "Content-Length: 219\n"
           "\n"
           "software: Nutch 1.0-dev (modified for clueweb09)\n"
           "isPartOf: clueweb09-en\n"
           "description: clueweb09 crawl with WARC output\n"
           "format: WARC file version 0.18\n"
           "conformsTo: "
           "http://www.archive.org/documents/WarcFileFormat-0.18.html\n"
           "\n";
}

TEST_CASE("Parse warcinfo record", "[warc][unit]")
{
    std::istringstream in(warcinfo());
    auto record = read_record(in);
    REQUIRE(std::get_if<Record>(&record) != nullptr);
    REQUIRE(in.peek() == EOF);
}

std::string response() {
    return "WARC/1.0\r\n"
           "WARC-Type: response\r\n"
           "WARC-Date: 2012-02-10T22:27:49Z\r\n"
           "WARC-TREC-ID: clueweb12-0000tw-00-00055\r\n"
           "WARC-Target-URI: http://rajakarcis.com/cms/xmlrpc.php\r\n"
           "WARC-Payload-Digest: sha1:QJ2RUVPQN37T3VVVCHHIUV4IWGVPF6BE\r\n"
           "WARC-IP-Address: 103.246.184.36\r\n"
           "WARC-Record-ID: <urn:uuid:5262e3ba-a830-45f2-85ad-cc5c90a213d9>\r\n"
           "Content-Type: application/http; msgtype=response\r\n"
           "Content-Length: 329\r\n"
           "\r\n"
           "HTTP/1.1 200 OK\r\n"
           "Server: lumanau.web.id\r\n"
           "Date: Fri, 10 Feb 2012 22:27:52 GMT\r\n"
           "Content-Type: text/plain\r\n"
           "Connection: close\r\n"
           "X-Powered-By: PHP/5.3.8\r\n"
           "Set-Cookie: "
           "w3tc_referrer=http%3A%2F%2Frajakarcis.com%2F2012%2F02%2F07%2Fgbh-the-england-legend-"
           "punk-rock%2F; path=/cms/\r\n"
           "Cluster: vm-2\r\n"
           "\r\n"
           "XML-RPC server accepts POST requests only.\r\n"
           "\r\n";
}

TEST_CASE("Parse response record", "[warc][unit]") {
    std::istringstream in(response());
    auto rec = read_record(in);
    Record *record = std::get_if<Record>(&rec);
    CHECK(record != nullptr);
    CHECK(in.peek() == EOF);
    CHECK(record->type() == "response");
    CHECK(record->content() ==
          "HTTP/1.1 200 OK\r\n"
          "Server: lumanau.web.id\r\n"
          "Date: Fri, 10 Feb 2012 22:27:52 GMT\r\n"
          "Content-Type: text/plain\r\n"
          "Connection: close\r\n"
          "X-Powered-By: PHP/5.3.8\r\n"
          "Set-Cookie: "
          "w3tc_referrer=http%3A%2F%2Frajakarcis.com%2F2012%2F02%2F07%2Fgbh-the-england-legend-"
          "punk-rock%2F; path=/cms/\r\n"
          "Cluster: vm-2\r\n"
          "\r\n"
          "XML-RPC server accepts POST requests only.");
    CHECK(record->url() == "http://rajakarcis.com/cms/xmlrpc.php");
    CHECK(record->trecid() == "clueweb12-0000tw-00-00055");
}

TEST_CASE("Check if parsed record is valid (has required fields)", "[warc][unit]")
{
    auto [input, valid] =
        GENERATE(table<std::string, bool>({{warcinfo(), false}, {response(), true}}));
    GIVEN("A record that can be parsed") {
        std::istringstream in(input);
        WHEN("Parse fields") {
            auto rec = read_record(in);
            Record *record = std::get_if<Record>(&rec);
            THEN("The record is " << (valid ? "valid" : "invalid")) {
                CHECK(record->valid_response() == valid);
            }
        }
    }
}

TEST_CASE("Parse multiple records", "[warc][unit]")
{
    auto function_type = GENERATE(as<std::string>(), "read_record", "read_subsequent_record");
    GIVEN("Two records and function: " << function_type) {
        std::istringstream in(
            "WARC/0.18\n"
            "WARC-Type: response\n"
            "WARC-Target-URI: http://00000-nrt-realestate.homepagestartup.com/\n"
            "WARC-Warcinfo-ID: 993d3969-9643-4934-b1c6-68d4dbe55b83\n"
            "WARC-Date: 2009-03-65T08:43:19-0800\n"
            "WARC-Record-ID: <urn:uuid:67f7cabd-146c-41cf-bd01-04f5fa7d5229>\n"
            "WARC-TREC-ID: clueweb09-en0000-00-00000\n"
            "Content-Type: application/http;msgtype=response\n"
            "WARC-Identified-Payload-Type: \n"
            "Content-Length: 27\n"
            "\n"
            "HTTP_HEADER1\n"
            "\n"
            "HTTP_CONTENT1\n"
            "\n"
            "WARC/0.18\n"
            "WARC-Type: response\n"
            "WARC-Target-URI: http://00000-nrt-realestate.homepagestartup.com/\n"
            "WARC-Warcinfo-ID: 993d3969-9643-4934-b1c6-68d4dbe55b83\n"
            "WARC-Date: 2009-03-65T08:43:19-0800\n"
            "WARC-Record-ID: <urn:uuid:67f7cabd-146c-41cf-bd01-04f5fa7d5229>\n"
            "WARC-TREC-ID: clueweb09-en0000-00-00000\n"
            "Content-Type: application/http;msgtype=response\n"
            "WARC-Identified-Payload-Type: \n"
            "Content-Length: 27\n"
            "\n"
            "HTTP_HEADER2\n"
            "\n"
            "HTTP_CONTENT2");
        if (function_type == "read_record") {
            auto record = read_record(in);
            CHECK(std::get_if<Record>(&record)->content() == "HTTP_HEADER1\n\nHTTP_CONTENT1");
            record = read_record(in);
            CHECK(std::get_if<Record>(&record)->content() == "HTTP_HEADER2\n\nHTTP_CONTENT2");
        } else {
            auto record = read_subsequent_record(in);
            CHECK(std::get_if<Record>(&record)->content() == "HTTP_HEADER1\n\nHTTP_CONTENT1");
            record = read_subsequent_record(in);
            CHECK(std::get_if<Record>(&record)->content() == "HTTP_HEADER2\n\nHTTP_CONTENT2");
        }
    }
}

TEST_CASE("Parse empty record", "[warc][unit]")
{
    std::istringstream in("\n");
    auto record = read_record(in);
    REQUIRE(std::get_if<Error>(&record) != nullptr);
    REQUIRE(std::get_if<Invalid_Version>(std::get_if<Error>(&record)) != nullptr);
}

TEST_CASE("Skip corrupted record", "[warc][unit]")
{
    GIVEN("Two records") {
        std::istringstream in(
            "WARC/0.18\n"
            "WARC-Type: response\n"
            "WARC-Target-URI: http://00000-nrt-realestate.homepagestartup.com/\n"
            "WARC-Warcinfo-ID: 993d3969-9643-4934-b1c6-68d4dbe55b83\n"
            "WARC-Date: 2009-03-65T08:43:19-0800\n\n" // <- corrupted
            "WARC-Record-ID: <urn:uuid:67f7cabd-146c-41cf-bd01-04f5fa7d5229>\n"
            "WARC-TREC-ID: clueweb09-en0000-00-00000\n"
            "Content-Type: application/http;msgtype=response\n"
            "WARC-Identified-Payload-Type: \n"
            "Content-Length: 27\n"
            "\n"
            "HTTP_HEADER1\n"
            "\n"
            "HTTP_CONTENT1\n"
            "\n"
            "WARC/0.18\n"
            "WARC-Type: response\n"
            "WARC-Target-URI: http://00000-nrt-realestate.homepagestartup.com/\n"
            "WARC-Warcinfo-ID: 993d3969-9643-4934-b1c6-68d4dbe55b83\n"
            "WARC-Date: 2009-03-65T08:43:19-0800\n"
            "WARC-Record-ID: <urn:uuid:67f7cabd-146c-41cf-bd01-04f5fa7d5229>\n"
            "WARC-TREC-ID: clueweb09-en0000-00-00000\n"
            "Content-Type: application/http;msgtype=response\n"
            "WARC-Identified-Payload-Type: \n"
            "Content-Length: 27\n"
            "\n"
            "HTTP_HEADER2\n"
            "\n"
            "HTTP_CONTENT2");
        WHEN("Read first record")
        {
            auto record = read_record(in);
            THEN("Record is invalid") {
                REQUIRE(std::get_if<Error>(&record) != nullptr);
                REQUIRE(std::get_if<Missing_Mandatory_Fields>(std::get_if<Error>(&record)) !=
                        nullptr);
            }
            AND_WHEN("Read following record") {
                record = read_subsequent_record(in);
                THEN("Record is valid") {
                    REQUIRE(std::get_if<Record>(&record) != nullptr);
                    REQUIRE(std::get_if<Record>(&record)->content() ==
                            "HTTP_HEADER2\n\nHTTP_CONTENT2");
                }
                AND_WHEN("Read another record") {
                    record = read_subsequent_record(in);
                    THEN("No more records found") {
                        REQUIRE(std::get_if<Error>(&record) != nullptr);
                        REQUIRE(std::get_if<Invalid_Version>(std::get_if<Error>(&record)) !=
                                nullptr);
                    }
                }
            }
        }
    }
}
