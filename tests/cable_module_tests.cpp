#include "CableImporter.h"
#include "CableLabelCodec.h"
#include "CableLabelPrinter.h"
#include "DatabaseManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QScopedPointer>
#include <QSqlQueryModel>
#include <QTemporaryDir>
#include <QTest>

#include <QtGui/private/qzipwriter_p.h>

class CableModuleTests : public QObject
{
    Q_OBJECT

private slots:
    void parsesCsvCableLedger();
    void parsesXlsxCableLedger();
    void acceptsXlsCableLedgerExtension();
    void parsesCableQrPayload();
    void rendersCableLabelPreview();
    void importsCableRowsWithUpsert();
    void borrowsAndReturnsCableBatch();
};

void CableModuleTests::parsesCsvCableLedger()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("cables.csv"));

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("\xEF\xBB\xBF编号,始端,终端\nDL-001,A柜,B柜\nDL-002,C柜,D柜\n");
    file.close();

    QString error;
    const QList<CableImportRow> rows = CableImporter::readFile(path, &error);

    QCOMPARE(error, QString());
    QCOMPARE(rows.size(), 2);
    QCOMPARE(rows.at(0).code, QStringLiteral("DL-001"));
    QCOMPARE(rows.at(0).startPoint, QStringLiteral("A柜"));
    QCOMPARE(rows.at(0).endPoint, QStringLiteral("B柜"));
    QCOMPARE(rows.at(1).code, QStringLiteral("DL-002"));
}

void CableModuleTests::parsesXlsxCableLedger()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("cables.xlsx"));

    QZipWriter zip(path);
    QVERIFY(zip.isWritable());
    zip.addFile(QStringLiteral("[Content_Types].xml"),
                QByteArrayLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                                  "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
                                  "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
                                  "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
                                  "<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>"
                                  "<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>"
                                  "</Types>"));
    zip.addFile(QStringLiteral("_rels/.rels"),
                QByteArrayLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                                  "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
                                  "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>"
                                  "</Relationships>"));
    zip.addFile(QStringLiteral("xl/workbook.xml"),
                QByteArrayLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                                  "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
                                  "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
                                  "<sheets><sheet name=\"Sheet1\" sheetId=\"1\" r:id=\"rId1\"/></sheets>"
                                  "</workbook>"));
    zip.addFile(QStringLiteral("xl/_rels/workbook.xml.rels"),
                QByteArrayLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                                  "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
                                  "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>"
                                  "</Relationships>"));
    zip.addFile(QStringLiteral("xl/worksheets/sheet1.xml"),
                QByteArrayLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                                  "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
                                  "<sheetData>"
                                  "<row r=\"1\"><c r=\"A1\" t=\"inlineStr\"><is><t>编号</t></is></c><c r=\"B1\" t=\"inlineStr\"><is><t>始端</t></is></c><c r=\"C1\" t=\"inlineStr\"><is><t>终端</t></is></c></row>"
                                  "<row r=\"2\"><c r=\"A2\" t=\"inlineStr\"><is><t>DL-101</t></is></c><c r=\"B2\" t=\"inlineStr\"><is><t>一号柜</t></is></c><c r=\"C2\" t=\"inlineStr\"><is><t>二号柜</t></is></c></row>"
                                  "</sheetData>"
                                  "</worksheet>"));
    zip.close();

    QString error;
    const QList<CableImportRow> rows = CableImporter::readFile(path, &error);

    QCOMPARE(error, QString());
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.at(0).code, QStringLiteral("DL-101"));
    QCOMPARE(rows.at(0).startPoint, QStringLiteral("一号柜"));
    QCOMPARE(rows.at(0).endPoint, QStringLiteral("二号柜"));
}

void CableModuleTests::acceptsXlsCableLedgerExtension()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("cables.xls"));

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("not a real xls file");
    file.close();

    QString error;
    const QList<CableImportRow> rows = CableImporter::readFile(path, &error);

    QVERIFY(rows.isEmpty());
    QVERIFY(!error.contains(QStringLiteral(".xlsx")));
}

void CableModuleTests::parsesCableQrPayload()
{
    CableRecord record;
    record.code = QStringLiteral("DL-9001");
    record.startPoint = QStringLiteral("A-01");
    record.endPoint = QStringLiteral("B-02");

    const QString payload = CableLabelCodec::encodePayload(record);
    QCOMPARE(payload, QStringLiteral("CABLE1|DL-9001|A-01|B-02"));

    CableQrScanData decoded;
    QString error;
    QVERIFY2(CableLabelCodec::decodePayload(payload, &decoded, &error), qPrintable(error));
    QCOMPARE(decoded.code, QStringLiteral("DL-9001"));
    QCOMPARE(decoded.startPoint, QStringLiteral("A-01"));
    QCOMPARE(decoded.endPoint, QStringLiteral("B-02"));

    QVERIFY2(CableLabelCodec::decodePayload(QStringLiteral("DL-9002"), &decoded, &error), qPrintable(error));
    QCOMPARE(decoded.code, QStringLiteral("DL-9002"));
    QCOMPARE(decoded.startPoint, QString());
    QCOMPARE(decoded.endPoint, QString());
}

void CableModuleTests::rendersCableLabelPreview()
{
    const QImage image = CableLabelPrinter::renderPreview(QStringLiteral("DL-9001"),
                                                          QStringLiteral("A-01"),
                                                          QStringLiteral("B-02"));
    QVERIFY(!image.isNull());
    QVERIFY(image.width() >= 400);
    QVERIFY(image.height() >= 250);
}

void CableModuleTests::importsCableRowsWithUpsert()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    DatabaseManager db;
    db.setDatabasePath(dir.filePath(QStringLiteral("equipment.db")));
    QVERIFY2(db.open(), qPrintable(db.lastError()));

    QList<CableImportRow> firstImport = {
        {QStringLiteral("DL-001"), QStringLiteral("A柜"), QStringLiteral("B柜")},
        {QStringLiteral("DL-002"), QStringLiteral("C柜"), QStringLiteral("D柜")}
    };
    CableImportSummary firstSummary;
    QVERIFY2(db.importCables(firstImport, &firstSummary), qPrintable(db.lastError()));
    QCOMPARE(firstSummary.inserted, 2);
    QCOMPARE(firstSummary.updated, 0);
    QCOMPARE(firstSummary.skipped, 0);

    QList<CableImportRow> secondImport = {
        {QStringLiteral("DL-001"), QStringLiteral("A柜"), QStringLiteral("B柜")},
        {QString(), QStringLiteral("空"), QStringLiteral("空")},
        {QStringLiteral("DL-003"), QStringLiteral("E柜"), QStringLiteral("F柜")}
    };
    CableImportSummary secondSummary;
    QVERIFY2(db.importCables(secondImport, &secondSummary), qPrintable(db.lastError()));
    QCOMPARE(secondSummary.inserted, 1);
    QCOMPARE(secondSummary.updated, 1);
    QCOMPARE(secondSummary.skipped, 1);

    const CableRecord first = db.cableByCode(QStringLiteral("DL-001"));
    QCOMPARE(first.startPoint, QStringLiteral("A柜"));
    QCOMPARE(first.endPoint, QStringLiteral("B柜"));
    QCOMPARE(first.status, QStringLiteral("在库"));
    QVERIFY(db.cableIdByCode(QStringLiteral("DL-002")) > 0);
    QVERIFY(db.cableIdByCode(QStringLiteral("DL-003")) > 0);
}

void CableModuleTests::borrowsAndReturnsCableBatch()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    DatabaseManager db;
    db.setDatabasePath(dir.filePath(QStringLiteral("equipment.db")));
    QVERIFY2(db.open(), qPrintable(db.lastError()));
    CableImportSummary summary;
    QVERIFY2(db.importCables({
                 {QStringLiteral("DL-001"), QStringLiteral("A柜"), QStringLiteral("B柜")},
                 {QStringLiteral("DL-002"), QStringLiteral("C柜"), QStringLiteral("D柜")}
             },
             &summary),
             qPrintable(db.lastError()));

    const int cable1 = db.cableIdByCode(QStringLiteral("DL-001"));
    const int cable2 = db.cableIdByCode(QStringLiteral("DL-002"));
    QVERIFY(cable1 > 0);
    QVERIFY(cable2 > 0);

    CableBorrowRecord record;
    record.borrower = QStringLiteral("张三");
    record.department = QStringLiteral("装配部");
    record.borrowDate = QDate(2026, 5, 20);
    record.expectedReturnDate = QDate(2026, 5, 27);
    record.remark = QStringLiteral("批量借出");

    QVERIFY2(db.borrowCables({cable1, cable2}, record), qPrintable(db.lastError()));
    QCOMPARE(db.cableStatus(cable1), QStringLiteral("借出"));
    QCOMPARE(db.cableStatus(cable2), QStringLiteral("借出"));

    QVERIFY2(db.returnCables({cable1, cable2}, QDate(2026, 5, 21), QStringLiteral("批量归还")),
             qPrintable(db.lastError()));
    QCOMPARE(db.cableStatus(cable1), QStringLiteral("在库"));
    QCOMPARE(db.cableStatus(cable2), QStringLiteral("在库"));

    QScopedPointer<QSqlQueryModel> returned(db.createCableBorrowModel(QString(), QStringLiteral("已归还"), nullptr));
    QCOMPARE(returned->rowCount(), 2);
}

QTEST_MAIN(CableModuleTests)
#include "cable_module_tests.moc"
