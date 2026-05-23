#pragma once

#include <QList>
#include <QDate>
#include <QString>

struct CableImportRow
{
    QString code;
    QString startPoint;
    QString endPoint;
    QDate usageExpiryDate;
    QString remark;
    bool usageExpiryDateSpecified = false;
    bool remarkSpecified = false;
};

struct CableImportSummary
{
    int inserted = 0;
    int updated = 0;
    int skipped = 0;
};

class CableImporter
{
public:
    static QList<CableImportRow> readFile(const QString &path, QString *errorMessage = nullptr);
};
