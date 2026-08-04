set(PROTOCOLS_SOURCES
    "${CMAKE_SOURCE_DIR}/src/core/Logger.cpp"
    "${CMAKE_SOURCE_DIR}/src/protocols/MimeParser.cpp"
)

mailinium_add_test(
    NAME MimeParserTest
    SOURCES MimeParserTest.cpp ${PROTOCOLS_SOURCES}
    LINKS Qt6::Core
)
