#include <QTest>
#include "protocols/MimeParser.h"

class MimeParserTest : public QObject
{
    Q_OBJECT

private slots:
    void testQuotedPrintableDecoding()
    {
        QByteArray encoded = "Hello=20World=21=0D=0AThis=20is=20a=20test=";
        QByteArray decoded = MimeParser::decodeQuotedPrintable(encoded);
        QVERIFY(decoded.contains("Hello World!"));
        QVERIFY(decoded.contains("This is a test"));
    }

    void testHeaderValueDecoding()
    {
        QString encodedHeader = QStringLiteral("=?UTF-8?B?VGVzdCBTdWJqZWN0?= =?UTF-8?Q?=20UTF-8?=");
        QString decoded = MimeParser::decodeHeaderValue(encodedHeader);
        QCOMPARE(decoded, QStringLiteral("Test Subject UTF-8"));
    }

    void testPlainTextParsing()
    {
        QByteArray raw = "Content-Type: text/plain; charset=utf-8\r\n"
                         "Content-Transfer-Encoding: 7bit\r\n\r\n"
                         "Hello from Mailinium!";

        MimeMessage msg = MimeParser::parse(raw);
        QCOMPARE(msg.plainBody, QStringLiteral("Hello from Mailinium!"));
        QVERIFY(msg.htmlBody.contains(QStringLiteral("Hello from Mailinium!")));
        QCOMPARE(msg.attachments.size(), 0);
    }

    void testMultipartAlternativeParsing()
    {
        QByteArray raw = "Content-Type: multipart/alternative; boundary=\"BOUNDARY123\"\r\n\r\n"
                         "--BOUNDARY123\r\n"
                         "Content-Type: text/plain; charset=utf-8\r\n\r\n"
                         "Plain text version\r\n"
                         "--BOUNDARY123\r\n"
                         "Content-Type: text/html; charset=utf-8\r\n\r\n"
                         "<h1>HTML version</h1>\r\n"
                         "--BOUNDARY123--\r\n";

        MimeMessage msg = MimeParser::parse(raw);
        QCOMPARE(msg.plainBody.trimmed(), QStringLiteral("Plain text version"));
        QCOMPARE(msg.htmlBody.trimmed(), QStringLiteral("<h1>HTML version</h1>"));
    }

    void testMultipartMixedWithAttachment()
    {
        QByteArray raw = "Content-Type: multipart/mixed; boundary=\"MIXED123\"\r\n\r\n"
                         "--MIXED123\r\n"
                         "Content-Type: text/plain\r\n\r\n"
                         "Email body\r\n"
                         "--MIXED123\r\n"
                         "Content-Type: application/pdf; name=\"doc.pdf\"\r\n"
                         "Content-Disposition: attachment; filename=\"doc.pdf\"\r\n"
                         "Content-Transfer-Encoding: base64\r\n\r\n"
                         "SGVsbG8gUERG\r\n"
                         "--MIXED123--\r\n";

        MimeMessage msg = MimeParser::parse(raw);
        QCOMPARE(msg.plainBody.trimmed(), QStringLiteral("Email body"));
        QCOMPARE(msg.attachments.size(), 1);
        QCOMPARE(msg.attachments.first().filename, QStringLiteral("doc.pdf"));
        QCOMPARE(msg.attachments.first().contentType, QStringLiteral("application/pdf"));
        QCOMPARE(msg.attachments.first().data, QByteArray("Hello PDF"));
    }

    void testNestedMultipart()
    {
        QByteArray raw = "Content-Type: multipart/mixed; boundary=\"MIXED123\"\r\n\r\n"
                         "--MIXED123\r\n"
                         "Content-Type: multipart/alternative; boundary=\"ALT123\"\r\n\r\n"
                         "--ALT123\r\n"
                         "Content-Type: text/plain; charset=utf-8\r\n\r\n"
                         "Nested text\r\n"
                         "--ALT123\r\n"
                         "Content-Type: text/html; charset=utf-8\r\n\r\n"
                         "<p>Nested HTML</p>\r\n"
                         "--ALT123--\r\n"
                         "--MIXED123\r\n"
                         "Content-Type: text/plain; name=\"note.txt\"\r\n"
                         "Content-Disposition: attachment; filename=\"note.txt\"\r\n\r\n"
                         "Attachment content\r\n"
                         "--MIXED123--\r\n";

        MimeMessage msg = MimeParser::parse(raw);
        QCOMPARE(msg.plainBody.trimmed(), QStringLiteral("Nested text"));
        QCOMPARE(msg.htmlBody.trimmed(), QStringLiteral("<p>Nested HTML</p>"));
        QCOMPARE(msg.attachments.size(), 1);
        QCOMPARE(msg.attachments.first().filename, QStringLiteral("note.txt"));
        QCOMPARE(msg.attachments.first().data.trimmed(), QByteArray("Attachment content"));
    }
};

QTEST_MAIN(MimeParserTest)
#include "MimeParserTest.moc"
