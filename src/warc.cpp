#include <memory>
#include <optional>
#include <string>

#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>

#include <warcpp/warcpp.hpp>

using warcpp::Error;
using warcpp::Invalid_Version;
using warcpp::match;
using warcpp::Record;
using warcpp::Result;
using json = nlohmann::json;

template <class Fn>
void read(std::istream &is, Fn print_record)
{
    while (not is.eof()) {
        match(
            warcpp::read_subsequent_record(is),
            [&](Record const &rec) { print_record(rec); },
            [&](Error const &error) { std::clog << "Invalid version in line: " << error << '\n'; });
    }
}

auto select_print_fn(std::string const &fmt)
    -> std::function<std::function<void(Record const &)>(std::ostream &)>
{
    if (fmt == "tsv") {
        return [](std::ostream &os) {
            return [&](Record const &rec) {
                if (rec.valid_response()) {
                    os << rec.trecid() << '\t' << rec.url() << '\t';
                    std::istringstream is(std::move(rec.content()));
                    std::string line;
                    while (std::getline(is, line)) {
                        os << "\\u000A" << line;
                    }
                    os << '\n';
                }
            };
        };
    } else if (fmt == "json") {
        return [](std::ostream &os) {
            return [&](Record const &rec) {
                if (rec.valid_response()) {
                    json entry = {
                        {"title", rec.trecid()}, {"url", rec.url()}, {"body", rec.content()}};
                    os << entry.dump(-1, ' ', false, nlohmann::json::error_handler_t::ignore)
                       << "\n";
                }
            };
        };
    } else {
        throw std::runtime_error("Unknown output format");
    }
}

int main(int argc, char **argv)
{
    std::string input;
    std::optional<std::string> output = std::nullopt;
    std::string fmt = "tsv";
    CLI::App app{
        "Parse a WARC file and output in a selected text format.\n\n"
        "Because lines delimit records, any new line characters in the content\n"
        "will be replaced by \\u000A sequence."};
    app.add_option("input", input, "Input file(s); use - to read from stdin")->required();
    app.add_option("output", output, "Output file; if missing, write to stdout");
    app.add_option("-f,--format", fmt, "Output file format", true)
        ->check(CLI::IsMember({"tsv", "json"}));
    CLI11_PARSE(app, argc, argv);

    auto print = select_print_fn(fmt);

    std::istream *is = &std::cin;
    std::unique_ptr<std::ifstream> file_is = nullptr;
    if (input != "-") {
        file_is = std::make_unique<std::ifstream>(input);
        is = file_is.get();
    }

    std::ostream *os = &std::cout;
    std::unique_ptr<std::ofstream> file_os = nullptr;
    if (output) {
        file_os = std::make_unique<std::ofstream>(*output);
        os = file_os.get();
    }

    read(*is, print(*os));
    return 0;
}
