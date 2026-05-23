#include "CableImporter.h"
#include "CableLabelCodec.h"
#include "CableScanSupport.h"
#include "CableLabelPrinter.h"
#include "DatabaseManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QScopedPointer>
#include <QSqlDatabase>
#include <QSqlQuery>
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
    void parsesGenericCableQrPayload();
    void resolvesScannedCableAndValidatesMode();
    void rendersCableLabelPreview();
    void migratesCableSchemaAndSwapsEndpointsOnce();
    void savesAndDeletesCableLedgerRecords();
    void importsCableRowsWithUpsert();
    void importsLargeCableBatchWithUpsert();
    void borrowsAndReturnsCableBatch();
    void listsOverdueCableBorrows();
};

void CableModuleTests::parsesCsvCableLedger()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("cables.csv"));

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("\xEF\xBB\xBF编号,始端,终端,使用期限,备注\nDL-001,A柜,B柜,2028-12-31,主用\nDL-002,C柜,D柜,,\n");
    file.close();

    QString error;
    const QList<CableImportRow> rows = CableImporter::readFile(path, &error);

    QCOMPARE(error, QString());
    QCOMPARE(rows.size(), 2);
    QCOMPARE(rows.at(0).code, QStringLiteral("DL-001"));
    QCOMPARE(rows.at(0).startPoint, QStringLiteral("A柜"));
    QCOMPARE(rows.at(0).endPoint, QStringLiteral("B柜"));
    QCOMPARE(rows.at(0).usageExpiryDate, QDate(2028, 12, 31));
    QCOMPARE(rows.at(0).remark, QStringLiteral("主用"));
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
    QCOMPARE(payload, QStringLiteral("DL-9001"));

    CableQrScanData decoded;
    QString error;
    QVERIFY2(CableLabelCodec::decodePayload(payload, &decoded, &error), qPrintable(error));
    QCOMPARE(decoded.code, QStringLiteral("DL-9001"));
    QCOMPARE(decoded.startPoint, QString());
    QCOMPARE(decoded.endPoint, QString());

    QVERIFY2(CableLabelCodec::decodePayload(QStringLiteral("CABLE1|DL-9001|A-01|B-02"), &decoded, &error),
             qPrintable(error));
    QCOMPARE(decoded.code, QStringLiteral("DL-9001"));
    QCOMPARE(decoded.startPoint, QStringLiteral("A-01"));
    QCOMPARE(decoded.endPoint, QStringLiteral("B-02"));

    QVERIFY2(CableLabelCodec::decodePayload(QStringLiteral("DL-9002"), &decoded, &error), qPrintable(error));
    QCOMPARE(decoded.code, QStringLiteral("DL-9002"));
    QCOMPARE(decoded.startPoint, QString());
    QCOMPARE(decoded.endPoint, QString());
}

void CableModuleTests::parsesGenericCableQrPayload()
{
    DatabaseManager db;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    db.setDatabasePath(dir.filePath(QStringLiteral("equipment.db")));
    QVERIFY2(db.open(), qPrintable(db.lastError()));

    CableImportSummary summary;
    QVERIFY2(db.importCables({
                 {QStringLiteral("BH004"), QStringLiteral("SD004"), QStringLiteral("ZD004")},
             },
             &summary),
             qPrintable(db.lastError()));

    CableScanResolvedCable resolved;
    QString error;
    QVERIFY2(CableScanSupport::resolveScanText(db, QStringLiteral("ZEBRA|BH004|SD004|ZD004"), &resolved, &error),
             qPrintable(error));
    QCOMPARE(resolved.scanData.code, QStringLiteral("BH004"));
    QCOMPARE(resolved.scanData.startPoint, QStringLiteral("SD004"));
    QCOMPARE(resolved.scanData.endPoint, QStringLiteral("ZD004"));
    QCOMPARE(resolved.record.code, QStringLiteral("BH004"));
}

void CableModuleTests::resolvesScannedCableAndValidatesMode()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    DatabaseManager db;
    db.setDatabasePath(dir.filePath(QStringLiteral("equipment.db")));
    QVERIFY2(db.open(), qPrintable(db.lastError()));

    CableImportSummary summary;
    QVERIFY2(db.importCables({
                 {QStringLiteral("BH004"), QStringLiteral("SD004"), QStringLiteral("ZD004")},
             },
             &summary),
             qPrintable(db.lastError()));

    CableScanResolvedCable resolved;
    QString error;
    QVERIFY2(CableScanSupport::resolveScanText(db,
                                               QStringLiteral("ZEBRA|BH004|SD004|ZD004"),
                                               &resolved,
                                               &error),
             qPrintable(error));

    QCOMPARE(resolved.record.code, QStringLiteral("BH004"));
    QCOMPARE(resolved.record.startPoint, QStringLiteral("SD004"));
    QCOMPARE(resolved.record.endPoint, QStringLiteral("ZD004"));
    QVERIFY(resolved.record.id > 0);

    QVERIFY2(CableScanSupport::canQueueForMode(CableScanMode::Borrow, resolved.record, &error), qPrintable(error));
    QVERIFY(!CableScanSupport::canQueueForMode(CableScanMode::Return, resolved.record, &error));
    QVERIFY(!error.isEmpty());

    CableBorrowRecord borrowRecord;
    borrowRecord.borrower = QStringLiteral("张三");
    borrowRecord.borrowDate = QDate(2026, 5, 20);
    QVERIFY2(db.borrowCables({resolved.record.id}, borrowRecord), qPrintable(db.lastError()));

    const CableRecord borrowed = db.cable(resolved.record.id);
    QVERIFY(!CableScanSupport::canQueueForMode(CableScanMode::Borrow, borrowed, &error));
    QVERIFY(CableScanSupport::canQueueForMode(CableScanMode::Return, borrowed, &error));
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

void CableModuleTests::migratesCableSchemaAndSwapsEndpointsOnce()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("equipment.db"));
    const QString connection = QStringLiteral("legacy_cable_migration_test");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
        db.setDatabaseName(path);
        QVERIFY(db.open());
        QSqlQuery query(db);
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE cables (id INTEGER PRIMARY KEY AUTOINCREMENT, code TEXT NOT NULL UNIQUE, "
            "start_point TEXT NOT NULL, end_point TEXT NOT NULL, status TEXT NOT NULL DEFAULT '在库')")));
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE cable_borrow_records (id INTEGER PRIMARY KEY AUTOINCREMENT, cable_id INTEGER NOT NULL, "
            "borrower TEXT NOT NULL, department TEXT, borrow_date TEXT NOT NULL, expected_return_date TEXT, "
            "actual_return_date TEXT, status TEXT NOT NULL DEFAULT '借出中', remark TEXT)")));
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO cables(code, start_point, end_point, status) VALUES('DL-OLD','START','END','在库')")));
        db.close();
    }
    QSqlDatabase::removeDatabase(connection);

    {
        DatabaseManager manager;
        manager.setDatabasePath(path);
        QVERIFY2(manager.open(), qPrintable(manager.lastError()));
        const CableRecord migrated = manager.cableByCode(QStringLiteral("DL-OLD"));
        QCOMPARE(migrated.startPoint, QStringLiteral("END"));
        QCOMPARE(migrated.endPoint, QStringLiteral("START"));
    }

    {
        DatabaseManager manager;
        manager.setDatabasePath(path);
        QVERIFY2(manager.open(), qPrintable(manager.lastError()));
        const CableRecord migrated = manager.cableByCode(QStringLiteral("DL-OLD"));
        QCOMPARE(migrated.startPoint, QStringLiteral("END"));
        QCOMPARE(migrated.endPoint, QStringLiteral("START"));
    }
}

void CableModuleTests::savesAndDeletesCableLedgerRecords()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    DatabaseManager db;
    db.setDatabasePath(dir.filePath(QStringLiteral("equipment.db")));
    QVERIFY2(db.open(), qPrintable(db.lastError()));

    CableRecord record;
    record.code = QStringLiteral("DL-EDIT");
    record.startPoint = QStringLiteral("S1");
    record.endPoint = QStringLiteral("E1");
    record.usageExpiryDate = QDate(2027, 12, 31);
    record.remark = QStringLiteral("initial");
    QVERIFY2(db.saveCable(record), qPrintable(db.lastError()));

    CableRecord saved = db.cableByCode(QStringLiteral("DL-EDIT"));
    QVERIFY(saved.id > 0);
    QCOMPARE(saved.startPoint, QStringLiteral("S1"));
    QCOMPARE(saved.endPoint, QStringLiteral("E1"));
    QCOMPARE(saved.usageExpiryDate, QDate(2027, 12, 31));
    QCOMPARE(saved.remark, QStringLiteral("initial"));

    saved.startPoint = QStringLiteral("S2");
    saved.endPoint = QStringLiteral("E2");
    saved.remark = QStringLiteral("updated");
    QVERIFY2(db.saveCable(saved), qPrintable(db.lastError()));
    saved = db.cableByCode(QStringLiteral("DL-EDIT"));
    QCOMPARE(saved.startPoint, QStringLiteral("S2"));
    QCOMPARE(saved.endPoint, QStringLiteral("E2"));
    QCOMPARE(saved.remark, QStringLiteral("updated"));

    CableBorrowRecord borrow;
    borrow.borrower = QStringLiteral("张三");
    borrow.borrowDate = QDate::currentDate();
    QVERIFY2(db.borrowCables({saved.id}, borrow), qPrintable(db.lastError()));
    QVERIFY(!db.removeCable(saved.id));
    QVERIFY(!db.lastError().isEmpty());

    CableRecord removable;
    removable.code = QStringLiteral("DL-REMOVE");
    removable.startPoint = QStringLiteral("S");
    removable.endPoint = QStringLiteral("E");
    QVERIFY2(db.saveCable(removable), qPrintable(db.lastError()));
    const int removableId = db.cableIdByCode(QStringLiteral("DL-REMOVE"));
    QVERIFY(removableId > 0);
    QVERIFY2(db.removeCable(removableId), qPrintable(db.lastError()));
    QCOMPARE(db.cableIdByCode(QStringLiteral("DL-REMOVE")), -1);
}

void CableModuleTests::importsCableRowsWithUpsert()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    DatabaseManager db;
    db.setDatabasePath(dir.filePath(QStringLiteral("equipment.db")));
    QVERIFY2(db.open(), qPrintable(db.lastError()));

    QList<CableImportRow> firstImport = {
        {QStringLiteral("DL-001"), QStringLiteral("A柜"), QStringLiteral("B柜"), QDate(2028, 1, 1), QStringLiteral("备用")},
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
    QCOMPARE(first.usageExpiryDate, QDate(2028, 1, 1));
    QCOMPARE(first.remark, QStringLiteral("备用"));
    QCOMPARE(first.status, QStringLiteral("在库"));
    QVERIFY(db.cableIdByCode(QStringLiteral("DL-002")) > 0);
    QVERIFY(db.cableIdByCode(QStringLiteral("DL-003")) > 0);
}

void CableModuleTests::importsLargeCableBatchWithUpsert()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    DatabaseManager db;
    db.setDatabasePath(dir.filePath(QStringLiteral("equipment.db")));
    QVERIFY2(db.open(), qPrintable(db.lastError()));

    QList<CableImportRow> firstImport;
    for (int i = 0; i < 3000; ++i) {
        firstImport.append({QStringLiteral("DL-%1").arg(i, 4, 10, QLatin1Char('0')),
                            QStringLiteral("A%1").arg(i),
                            QStringLiteral("B%1").arg(i)});
    }

    CableImportSummary firstSummary;
    QVERIFY2(db.importCables(firstImport, &firstSummary), qPrintable(db.lastError()));
    QCOMPARE(firstSummary.inserted, 3000);
    QCOMPARE(firstSummary.updated, 0);
    QCOMPARE(firstSummary.skipped, 0);

    QList<CableImportRow> secondImport = {
        {QStringLiteral("DL-0001"), QStringLiteral("A-new"), QStringLiteral("B-new")},
        {QString(), QStringLiteral("ignored"), QStringLiteral("ignored")},
        {QStringLiteral("DL-3000"), QStringLiteral("A3000"), QStringLiteral("B3000")}
    };

    CableImportSummary secondSummary;
    QVERIFY2(db.importCables(secondImport, &secondSummary), qPrintable(db.lastError()));
    QCOMPARE(secondSummary.inserted, 1);
    QCOMPARE(secondSummary.updated, 1);
    QCOMPARE(secondSummary.skipped, 1);

    const CableRecord updated = db.cableByCode(QStringLiteral("DL-0001"));
    QCOMPARE(updated.startPoint, QStringLiteral("A-new"));
    QCOMPARE(updated.endPoint, QStringLiteral("B-new"));
    QVERIFY(db.cableIdByCode(QStringLiteral("DL-3000")) > 0);

    QScopedPointer<QSqlQueryModel> model(db.createCableModel(QString(), QString(), nullptr));
    while (model->canFetchMore()) {
        model->fetchMore();
    }
    QCOMPARE(model->rowCount(), 3001);
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

void CableModuleTests::listsOverdueCableBorrows()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    DatabaseManager db;
    db.setDatabasePath(dir.filePath(QStringLiteral("equipment.db")));
    QVERIFY2(db.open(), qPrintable(db.lastError()));

    CableImportSummary summary;
    QVERIFY2(db.importCables({
                 {QStringLiteral("DL-LATE"), QStringLiteral("A"), QStringLiteral("B")},
                 {QStringLiteral("DL-OK"), QStringLiteral("C"), QStringLiteral("D")}
             },
             &summary),
             qPrintable(db.lastError()));

    const int late = db.cableIdByCode(QStringLiteral("DL-LATE"));
    const int ok = db.cableIdByCode(QStringLiteral("DL-OK"));
    QVERIFY(late > 0);
    QVERIFY(ok > 0);

    CableBorrowRecord overdueRecord;
    overdueRecord.borrower = QStringLiteral("李四");
    overdueRecord.borrowDate = QDate::currentDate().addDays(-10);
    overdueRecord.expectedReturnDate = QDate::currentDate().addDays(-1);
    QVERIFY2(db.borrowCables({late}, overdueRecord), qPrintable(db.lastError()));

    CableBorrowRecord activeRecord;
    activeRecord.borrower = QStringLiteral("王五");
    activeRecord.borrowDate = QDate::currentDate();
    activeRecord.expectedReturnDate = QDate::currentDate().addDays(7);
    QVERIFY2(db.borrowCables({ok}, activeRecord), qPrintable(db.lastError()));

    QCOMPARE(db.overdueCableBorrowCount(), 1);

    QScopedPointer<QSqlQueryModel> overdue(db.createOverdueCableBorrowModel(nullptr));
    QCOMPARE(overdue->rowCount(), 1);
    QCOMPARE(overdue->index(0, 0).data().toString(), QStringLiteral("DL-LATE"));

    QScopedPointer<QSqlQueryModel> filtered(db.createCableModel(QString(), QStringLiteral("逾期未还"), nullptr));
    QCOMPARE(filtered->rowCount(), 1);
    QCOMPARE(filtered->index(0, 1).data().toString(), QStringLiteral("DL-LATE"));
}

QTEST_MAIN(CableModuleTests)
#include "cable_module_tests.moc"
