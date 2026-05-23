#include "CableImporter.h"

#include <QDate>
#include <QFile>
#include <QFileInfo>
#include <QTextCodec>
#include <QTextStream>
#include <QXmlStreamReader>

#include <QAxObject>
#include <QDir>
#include <QtGui/private/qzipreader_p.h>

namespace {

void setError(QString *errorMessage, const QString &message)
{
    if (errorMessage) {
        *errorMessage = message;
    }
}

QString normalizedHeader(const QString &value)
{
    QString result = value.trimmed();
    if (result.startsWith(QChar(0xFEFF))) {
        result.remove(0, 1);
    }
    return result;
}

QList<QString> parseCsvLine(const QString &line)
{
    QList<QString> cells;
    QString cell;
    bool inQuotes = false;

    for (int i = 0; i < line.size(); ++i) {
        const QChar ch = line.at(i);
        if (ch == QLatin1Char('"')) {
            if (inQuotes && i + 1 < line.size() && line.at(i + 1) == QLatin1Char('"')) {
                cell.append(ch);
                ++i;
            } else {
                inQuotes = !inQuotes;
            }
        } else if (ch == QLatin1Char(',') && !inQuotes) {
            cells.append(cell.trimmed());
            cell.clear();
        } else {
            cell.append(ch);
        }
    }

    cells.append(cell.trimmed());
    return cells;
}

int headerIndex(const QList<QString> &headers, const QString &name)
{
    for (int i = 0; i < headers.size(); ++i) {
        if (normalizedHeader(headers.at(i)) == name) {
            return i;
        }
    }
    return -1;
}

QDate parseDateCell(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return QDate();
    }

    const QStringList formats = {
        QStringLiteral("yyyy-MM-dd"),
        QStringLiteral("yyyy/M/d"),
        QStringLiteral("yyyy/MM/dd"),
        QStringLiteral("yyyy.M.d"),
        QStringLiteral("yyyy.MM.dd")
    };
    for (const QString &format : formats) {
        const QDate date = QDate::fromString(trimmed, format);
        if (date.isValid()) {
            return date;
        }
    }

    bool ok = false;
    const int serial = trimmed.toInt(&ok);
    if (ok && serial > 0) {
        return QDate(1899, 12, 30).addDays(serial);
    }

    return QDate();
}

bool isEmptyRow(const QList<QString> &cells)
{
    for (const QString &cell : cells) {
        if (!cell.trimmed().isEmpty()) {
            return false;
        }
    }
    return true;
}

CableImportRow rowFromCells(const QList<QString> &cells,
                            int codeIndex,
                            int startIndex,
                            int endIndex,
                            int usageExpiryIndex,
                            int remarkIndex)
{
    CableImportRow row;
    row.code = codeIndex >= 0 && codeIndex < cells.size() ? cells.at(codeIndex).trimmed() : QString();
    row.startPoint = startIndex >= 0 && startIndex < cells.size() ? cells.at(startIndex).trimmed() : QString();
    row.endPoint = endIndex >= 0 && endIndex < cells.size() ? cells.at(endIndex).trimmed() : QString();
    row.usageExpiryDate = usageExpiryIndex >= 0 && usageExpiryIndex < cells.size()
                              ? parseDateCell(cells.at(usageExpiryIndex))
                              : QDate();
    row.remark = remarkIndex >= 0 && remarkIndex < cells.size() ? cells.at(remarkIndex).trimmed() : QString();
    row.usageExpiryDateSpecified = usageExpiryIndex >= 0;
    row.remarkSpecified = remarkIndex >= 0;
    return row;
}

QList<CableImportRow> readCsv(const QString &path, QString *errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(errorMessage, QStringLiteral("无法打开 CSV 文件：%1").arg(file.errorString()));
        return {};
    }

    const QByteArray data = file.readAll();
    QTextCodec *utf8 = QTextCodec::codecForName("UTF-8");
    QTextCodec::ConverterState state;
    QString text = utf8->toUnicode(data.constData(), data.size(), &state);
    if (state.invalidChars > 0) {
        text = QTextCodec::codecForLocale()->toUnicode(data);
    }

    QTextStream stream(&text, QIODevice::ReadOnly);
    QList<QString> headers;
    while (!stream.atEnd() && isEmptyRow(headers)) {
        headers = parseCsvLine(stream.readLine());
    }

    const int codeIndex = headerIndex(headers, QStringLiteral("编号"));
    const int startIndex = headerIndex(headers, QStringLiteral("始端"));
    const int endIndex = headerIndex(headers, QStringLiteral("终端"));
    const int usageExpiryIndex = headerIndex(headers, QStringLiteral("使用期限"));
    const int remarkIndex = headerIndex(headers, QStringLiteral("备注"));
    if (codeIndex < 0 || startIndex < 0 || endIndex < 0) {
        setError(errorMessage, QStringLiteral("导入文件必须包含表头：编号、始端、终端。"));
        return {};
    }

    QList<CableImportRow> rows;
    while (!stream.atEnd()) {
        CableImportRow row = rowFromCells(parseCsvLine(stream.readLine()),
                                          codeIndex,
                                          startIndex,
                                          endIndex,
                                          usageExpiryIndex,
                                          remarkIndex);
        if (!row.code.isEmpty()) {
            rows.append(row);
        }
    }
    return rows;
}

QStringList readSharedStrings(const QByteArray &xml)
{
    QStringList values;
    QXmlStreamReader reader(xml);
    QString current;
    bool inText = false;

    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement() && reader.name() == QStringLiteral("t")) {
            current.clear();
            inText = true;
        } else if (reader.isCharacters() && inText) {
            current += reader.text().toString();
        } else if (reader.isEndElement() && reader.name() == QStringLiteral("t")) {
            values.append(current);
            inText = false;
        }
    }
    return values;
}

int columnIndexFromCellRef(const QString &ref)
{
    int index = 0;
    for (const QChar ch : ref) {
        if (!ch.isLetter()) {
            break;
        }
        index = index * 26 + ch.toUpper().unicode() - QLatin1Char('A').unicode() + 1;
    }
    return qMax(0, index - 1);
}

QList<QList<QString>> readSheetRows(const QByteArray &xml, const QStringList &sharedStrings)
{
    QList<QList<QString>> rows;
    QXmlStreamReader reader(xml);
    QList<QString> currentRow;
    int currentColumn = -1;
    QString currentType;
    QString currentValue;
    QString inlineText;
    bool inValue = false;

    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement() && reader.name() == QStringLiteral("row")) {
            currentRow.clear();
        } else if (reader.isStartElement() && reader.name() == QStringLiteral("c")) {
            currentColumn = columnIndexFromCellRef(reader.attributes().value(QStringLiteral("r")).toString());
            currentType = reader.attributes().value(QStringLiteral("t")).toString();
            currentValue.clear();
            inlineText.clear();
        } else if (reader.isStartElement() && (reader.name() == QStringLiteral("v") || reader.name() == QStringLiteral("t"))) {
            currentValue.clear();
            inValue = true;
        } else if (reader.isCharacters() && inValue) {
            currentValue += reader.text().toString();
        } else if (reader.isEndElement() && (reader.name() == QStringLiteral("v") || reader.name() == QStringLiteral("t"))) {
            if (reader.name() == QStringLiteral("t")) {
                inlineText += currentValue;
            }
            inValue = false;
        } else if (reader.isEndElement() && reader.name() == QStringLiteral("c")) {
            QString text = currentValue.trimmed();
            if (currentType == QStringLiteral("s")) {
                bool ok = false;
                const int sharedIndex = text.toInt(&ok);
                if (ok && sharedIndex >= 0 && sharedIndex < sharedStrings.size()) {
                    text = sharedStrings.at(sharedIndex);
                }
            } else if (currentType == QStringLiteral("inlineStr")) {
                text = inlineText.trimmed();
            }

            while (currentRow.size() <= currentColumn) {
                currentRow.append(QString());
            }
            if (currentColumn >= 0) {
                currentRow[currentColumn] = text.trimmed();
            }
        } else if (reader.isEndElement() && reader.name() == QStringLiteral("row")) {
            rows.append(currentRow);
        }
    }
    return rows;
}

QString firstWorksheetPath(const QZipReader &zip)
{
    const QString workbookXml = QString::fromUtf8(zip.fileData(QStringLiteral("xl/workbook.xml")));
    QXmlStreamReader workbookReader(workbookXml);
    QString firstSheetRelId;
    while (!workbookReader.atEnd()) {
        workbookReader.readNext();
        if (workbookReader.isStartElement() && workbookReader.name() == QStringLiteral("sheet")) {
            firstSheetRelId = workbookReader.attributes().value(QStringLiteral("r:id")).toString();
            break;
        }
    }

    const QByteArray relsXml = zip.fileData(QStringLiteral("xl/_rels/workbook.xml.rels"));
    QXmlStreamReader relsReader(relsXml);
    while (!relsReader.atEnd()) {
        relsReader.readNext();
        if (!relsReader.isStartElement() || relsReader.name() != QStringLiteral("Relationship")) {
            continue;
        }
        if (relsReader.attributes().value(QStringLiteral("Id")).toString() != firstSheetRelId) {
            continue;
        }

        QString target = relsReader.attributes().value(QStringLiteral("Target")).toString();
        if (target.startsWith(QLatin1Char('/'))) {
            target.remove(0, 1);
        } else if (!target.startsWith(QStringLiteral("xl/"))) {
            target.prepend(QStringLiteral("xl/"));
        }
        return target;
    }

    return QStringLiteral("xl/worksheets/sheet1.xml");
}

QList<CableImportRow> readXlsx(const QString &path, QString *errorMessage)
{
    QZipReader zip(path);
    if (!zip.exists() || !zip.isReadable()) {
        setError(errorMessage, QStringLiteral("无法读取 XLSX 文件。"));
        return {};
    }

    const QStringList sharedStrings = readSharedStrings(zip.fileData(QStringLiteral("xl/sharedStrings.xml")));
    const QByteArray sheetXml = zip.fileData(firstWorksheetPath(zip));
    if (sheetXml.isEmpty()) {
        setError(errorMessage, QStringLiteral("XLSX 文件中未找到第一张工作表。"));
        return {};
    }

    const QList<QList<QString>> sheetRows = readSheetRows(sheetXml, sharedStrings);
    if (sheetRows.isEmpty()) {
        return {};
    }

    const QList<QString> headers = sheetRows.first();
    const int codeIndex = headerIndex(headers, QStringLiteral("编号"));
    const int startIndex = headerIndex(headers, QStringLiteral("始端"));
    const int endIndex = headerIndex(headers, QStringLiteral("终端"));
    const int usageExpiryIndex = headerIndex(headers, QStringLiteral("使用期限"));
    const int remarkIndex = headerIndex(headers, QStringLiteral("备注"));
    if (codeIndex < 0 || startIndex < 0 || endIndex < 0) {
        setError(errorMessage, QStringLiteral("导入文件必须包含表头：编号、始端、终端。"));
        return {};
    }

    QList<CableImportRow> rows;
    for (int i = 1; i < sheetRows.size(); ++i) {
        CableImportRow row = rowFromCells(sheetRows.at(i),
                                          codeIndex,
                                          startIndex,
                                          endIndex,
                                          usageExpiryIndex,
                                          remarkIndex);
        if (!row.code.isEmpty()) {
            rows.append(row);
        }
    }
    return rows;
}

bool hasOleCompoundHeader(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    return file.read(8).toHex().toLower() == QByteArrayLiteral("d0cf11e0a1b11ae1");
}

QList<QString> excelRowValues(QAxObject *sheet, int row, int columnCount)
{
    QList<QString> values;
    for (int column = 1; column <= columnCount; ++column) {
        QScopedPointer<QAxObject> cell(sheet->querySubObject("Cells(int,int)", row, column));
        values.append(cell ? cell->property("Value").toString().trimmed() : QString());
    }
    return values;
}

QList<CableImportRow> readXls(const QString &path, QString *errorMessage)
{
    if (!hasOleCompoundHeader(path)) {
        setError(errorMessage, QStringLiteral("无法读取 XLS 文件：文件不是有效的 Excel 97-2003 格式。"));
        return {};
    }

    QAxObject excel(QStringLiteral("Excel.Application"));
    if (excel.isNull()) {
        setError(errorMessage, QStringLiteral("无法读取 XLS 文件：请确认本机已安装 Microsoft Excel。"));
        return {};
    }

    excel.setProperty("Visible", false);
    excel.setProperty("DisplayAlerts", false);

    QScopedPointer<QAxObject> workbooks(excel.querySubObject("Workbooks"));
    QScopedPointer<QAxObject> workbook(workbooks ? workbooks->querySubObject("Open(const QString&)", QDir::toNativeSeparators(path)) : nullptr);
    if (!workbook) {
        excel.dynamicCall("Quit()");
        setError(errorMessage, QStringLiteral("无法打开 XLS 文件。"));
        return {};
    }

    QScopedPointer<QAxObject> sheet(workbook->querySubObject("Worksheets(int)", 1));
    QScopedPointer<QAxObject> usedRange(sheet ? sheet->querySubObject("UsedRange") : nullptr);
    QScopedPointer<QAxObject> rowsObject(usedRange ? usedRange->querySubObject("Rows") : nullptr);
    QScopedPointer<QAxObject> columnsObject(usedRange ? usedRange->querySubObject("Columns") : nullptr);
    const int rowCount = rowsObject ? rowsObject->property("Count").toInt() : 0;
    const int columnCount = columnsObject ? columnsObject->property("Count").toInt() : 0;

    if (rowCount <= 0 || columnCount <= 0) {
        workbook->dynamicCall("Close(Boolean)", false);
        excel.dynamicCall("Quit()");
        return {};
    }

    const QList<QString> headers = excelRowValues(sheet.data(), 1, columnCount);
    const int codeIndex = headerIndex(headers, QStringLiteral("编号"));
    const int startIndex = headerIndex(headers, QStringLiteral("始端"));
    const int endIndex = headerIndex(headers, QStringLiteral("终端"));
    const int usageExpiryIndex = headerIndex(headers, QStringLiteral("使用期限"));
    const int remarkIndex = headerIndex(headers, QStringLiteral("备注"));
    if (codeIndex < 0 || startIndex < 0 || endIndex < 0) {
        workbook->dynamicCall("Close(Boolean)", false);
        excel.dynamicCall("Quit()");
        setError(errorMessage, QStringLiteral("导入文件必须包含表头：编号、始端、终端。"));
        return {};
    }

    QList<CableImportRow> result;
    for (int row = 2; row <= rowCount; ++row) {
        CableImportRow record = rowFromCells(excelRowValues(sheet.data(), row, columnCount),
                                             codeIndex,
                                             startIndex,
                                             endIndex,
                                             usageExpiryIndex,
                                             remarkIndex);
        if (!record.code.isEmpty()) {
            result.append(record);
        }
    }

    workbook->dynamicCall("Close(Boolean)", false);
    excel.dynamicCall("Quit()");
    return result;
}

} // namespace

QList<CableImportRow> CableImporter::readFile(const QString &path, QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }

    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QStringLiteral("csv")) {
        return readCsv(path, errorMessage);
    }
    if (suffix == QStringLiteral("xlsx")) {
        return readXlsx(path, errorMessage);
    }
    if (suffix == QStringLiteral("xls")) {
        return readXls(path, errorMessage);
    }

    setError(errorMessage, QStringLiteral("仅支持 .xlsx、.xls 和 .csv 文件。"));
    return {};
}
