#pragma once

#include <QString>

struct CableRecord;

struct CableQrScanData
{
    QString code;
    QString startPoint;
    QString endPoint;
};

class CableLabelCodec
{
public:
    static QString encodePayload(const CableRecord &record);
    static QString encodePayload(const QString &code, const QString &startPoint, const QString &endPoint);
    static bool decodePayload(const QString &text, CableQrScanData *data, QString *errorMessage = nullptr);
    static QString displayText(const QString &code, const QString &startPoint, const QString &endPoint);
};
