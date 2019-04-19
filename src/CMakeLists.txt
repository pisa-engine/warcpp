add_executable(warc warc.cpp)
target_link_libraries(warc
  warcpp
  CLI11
)
