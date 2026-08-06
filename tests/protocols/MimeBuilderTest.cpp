#include <QtTest>
#include "protocols/MimeBuilder.h"

//
// MimeBuilderTest — unit tests for the MimeBuilder class.
//
class MimeBuilderTest : public QObject
{
    Q_OBJECT

private slots:
    // --- Plain text only --------------------------------------------------

    void plainTextOnly_containsTextPlainHeader()
    {
        ComposeData cd;
        cd.from      = QStringLiteral("sender@example.com");
        cd.to        = QStringLiteral("recipient@example.com");
        cd.subject   = QStringLiteral("Hello");
        cd.plainBody = QStringLiteral("This is a test.");

        const QByteArray raw = MimeBuilder::build(cd);
        QVERIFY(!raw.isEmpty());
        QVERIFY(raw.contains("Content-Type: text/plain"));
        QVERIFY(raw.contains("Content-Transfer-Encoding: quoted-printable"));
        QVERIFY(raw.contains("From: sender@example.com"));
        QVERIFY(raw.contains("To: recipient@example.com"));
        QVERIFY(raw.contains("Subject: Hello"));
    }

    void plainTextOnly_noAttachments_isNotMultipart()
    {
        ComposeData cd;
        cd.from      = QStringLiteral("a@example.com");
        cd.to        = QStringLiteral("b@example.com");
        cd.subject   = QStringLiteral("Test");
        cd.plainBody = QStringLiteral("Body.");

        const QByteArray raw = MimeBuilder::build(cd);
        QVERIFY(!raw.contains("multipart/mixed"));
        QVERIFY(!raw.contains("multipart/alternative"));
    }

    // --- HTML + plain (multipart/alternative) ----------------------------

    void htmlAndPlain_producesMultipartAlternative()
    {
        ComposeData cd;
        cd.from      = QStringLiteral("a@example.com");
        cd.to        = QStringLiteral("b@example.com");
        cd.subject   = QStringLiteral("Rich");
        cd.plainBody = QStringLiteral("Plain content");
        cd.htmlBody  = QStringLiteral("<p>HTML content</p>");

        const QByteArray raw = MimeBuilder::build(cd);
        QVERIFY(!raw.isEmpty());
        QVERIFY(raw.contains("multipart/alternative"));
        QVERIFY(raw.contains("text/plain"));
        QVERIFY(raw.contains("text/html"));
    }

    // --- Reply headers (In-Reply-To, References) --------------------------

    void replyHeaders_arePresent()
    {
        ComposeData cd;
        cd.from       = QStringLiteral("a@example.com");
        cd.to         = QStringLiteral("b@example.com");
        cd.subject    = QStringLiteral("Re: Test");
        cd.plainBody  = QStringLiteral("reply");
        cd.inReplyTo  = QStringLiteral("<original@example.com>");
        cd.references = QStringLiteral("<original@example.com>");

        const QByteArray raw = MimeBuilder::build(cd);
        QVERIFY(raw.contains("In-Reply-To: <original@example.com>"));
        QVERIFY(raw.contains("References: <original@example.com>"));
    }

    // --- RFC 2047 encoded subject -----------------------------------------

    void nonAsciiSubject_isEncoded()
    {
        ComposeData cd;
        cd.from      = QStringLiteral("a@example.com");
        cd.to        = QStringLiteral("b@example.com");
        cd.subject   = QStringLiteral("Ünïcödé subject");
        cd.plainBody = QStringLiteral("body");

        const QByteArray raw = MimeBuilder::build(cd);
        // Encoded words start with =?UTF-8?Q?
        QVERIFY(raw.contains("=?UTF-8?Q?"));
    }

    void asciiSubject_isNotEncoded()
    {
        ComposeData cd;
        cd.from      = QStringLiteral("a@example.com");
        cd.to        = QStringLiteral("b@example.com");
        cd.subject   = QStringLiteral("Simple ASCII subject");
        cd.plainBody = QStringLiteral("body");

        const QByteArray raw = MimeBuilder::build(cd);
        QVERIFY(raw.contains("Subject: Simple ASCII subject"));
    }

    // --- Cc / Bcc header handling -----------------------------------------

    void cc_isPresent_bcc_isAbsent()
    {
        ComposeData cd;
        cd.from      = QStringLiteral("a@example.com");
        cd.to        = QStringLiteral("b@example.com");
        cd.cc        = QStringLiteral("c@example.com");
        cd.bcc       = QStringLiteral("d@example.com");
        cd.subject   = QStringLiteral("Test");
        cd.plainBody = QStringLiteral("body");

        const QByteArray raw = MimeBuilder::build(cd);
        QVERIFY(raw.contains("Cc: c@example.com"));
        // BCC must NOT appear in headers
        QVERIFY(!raw.contains("Bcc:"));
        QVERIFY(!raw.contains("d@example.com"));
    }

    // --- Boundary generation is unique -----------------------------------

    void generateBoundary_isUnique()
    {
        const QByteArray b1 = MimeBuilder::generateBoundary();
        const QByteArray b2 = MimeBuilder::generateBoundary();
        QVERIFY(!b1.isEmpty());
        QVERIFY(!b2.isEmpty());
        QVERIFY(b1 != b2);
    }

    // --- MIME type detection ---------------------------------------------

    void detectMimeType_knownExtensions()
    {
        QCOMPARE(MimeBuilder::detectMimeType(QStringLiteral("file.pdf")),  QStringLiteral("application/pdf"));
        QCOMPARE(MimeBuilder::detectMimeType(QStringLiteral("image.png")), QStringLiteral("image/png"));
        QCOMPARE(MimeBuilder::detectMimeType(QStringLiteral("doc.docx")),  QStringLiteral("application/vnd.openxmlformats-officedocument.wordprocessingml.document"));
    }

    void detectMimeType_unknownExtension_isFallback()
    {
        QCOMPARE(MimeBuilder::detectMimeType(QStringLiteral("file.xyz")),
                 QStringLiteral("application/octet-stream"));
    }

    // --- encodeHeaderWord round-trip -------------------------------------

    void encodeHeaderWord_containsUtf8Prefix()
    {
        const QByteArray encoded = MimeBuilder::encodeHeaderWord(QStringLiteral("Héllo"));
        QVERIFY(encoded.startsWith("=?UTF-8?Q?"));
        QVERIFY(encoded.endsWith("?="));
    }

    void encodeAddress_noName_returnsEmailOnly()
    {
        QCOMPARE(MimeBuilder::encodeAddress(QStringLiteral(""), QStringLiteral("a@example.com")),
                 QStringLiteral("a@example.com"));
    }

    void encodeAddress_withName_returnsFormatted()
    {
        const QString result = MimeBuilder::encodeAddress(
            QStringLiteral("Jane Doe"), QStringLiteral("jane@example.com"));
        QCOMPARE(result, QStringLiteral("Jane Doe <jane@example.com>"));
    }
};

QTEST_MAIN(MimeBuilderTest)
#include "MimeBuilderTest.moc"
