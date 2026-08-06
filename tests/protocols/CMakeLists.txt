set(PROTOCOLS_SOURCES
    "${CMAKE_SOURCE_DIR}/src/core/Logger.cpp"
    "${CMAKE_SOURCE_DIR}/src/protocols/MimeParser.cpp"
    "${CMAKE_SOURCE_DIR}/src/protocols/MimeBuilder.cpp"
)

mailinium_add_test(
    NAME MimeParserTest
    SOURCES MimeParserTest.cpp ${PROTOCOLS_SOURCES}
    LINKS Qt6::Core
)

mailinium_add_test(
    NAME MimeBuilderTest
    SOURCES MimeBuilderTest.cpp ${PROTOCOLS_SOURCES}
    LINKS Qt6::Core
)

set(SMTP_SOURCES
    "${CMAKE_SOURCE_DIR}/src/core/Logger.cpp"
    "${CMAKE_SOURCE_DIR}/src/protocols/smtp/SmtpClient.cpp"
)

mailinium_add_test(
    NAME SmtpClientTest
    SOURCES SmtpClientTest.cpp ${SMTP_SOURCES}
    LINKS Qt6::Core Qt6::Network
)
