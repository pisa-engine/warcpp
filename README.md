warcpp
======

[![Build Status](https://travis-ci.org/pisa-engine/warcpp.svg?branch=master)](https://travis-ci.org/pisa-engine/warcpp)

A single-header C++ parser for the Web Archive (WARC) format.

## Usage

### `namespace warcpp`

```cpp
[[nodiscard]] auto read_record(std::istream&) -> Result;
```
Reads a record from the current position in the stream.

```cpp
[[nodiscard]] auto read_subsequent_record(std::istream&) -> Result;
```
Same as `read_record` but will skip any junk before the first
valid beginning of a record (WARC version line).

### Pattern Matching

`Result` is an alias for `std::variant<Record, Error>`.
For convenience, `match` function is provided to enable basic pattern matching.
The following example shows how to iterate over a WARC collection
and retrieve the total size of all valid records.

```cpp
#include <iostream>
#include <warcpp/warcpp.hpp>

using warcpp::match;
using warcpp::Result;
using warcpp::Error;
using warcpp::Record;
using warcpp::Invalid_Version;

std::size_t total_content_size(std::ifstream& is)
{
    std::size_t total{0u};
    while (not is.eof())
    {
        total += match(
            warcpp::read_subsequent_record(in), // will skip to first valid beginning
            [](Record const &rec) -> std::size_t {
                return rec.content_length();
            },
            [](Error const &error) -> std::size_t {
                match(error,
                      [](Invalid_Version const& err) {
                          std::clog << "Invalid version in line: " << err.version << '\n';
                      }
                      [](auto const& err) {
                          std::clog << "Some other error: " << error << '\n';
                      });
                return 0u;
            });
    }
    return total;
}
```

### Classic Approach

Alternatively, you can use `holds_record` and `std::get<T>(std::variant)`
functions to access returned data:

```cpp
#include <iostream>
#include <warcpp/warcpp.hpp>

using warcpp::holds_record;
using warcpp::as_record;
using warcpp::as_error;

std::size_t total_content_size(std::ifstream& is)
{
    std::size_t total{0u};
    while (not is.eof())
    {
        auto record = warcpp::read_subsequent_record(in);
        if (holds_record(record)) {
            total += std::get<Record>(record).content_length();
        }
        else {
            // You actually don't need to cast to print
            // if you do not care which error was returned.
            std::clog << "Warn: " << record << '\n';
        }
    }
    return total;
}
```
