#include "CableLabelCodec.h"

#include "DatabaseManager.h"

#include <QUrl>

namespace {

void setError(QString *errorMessage, const QString &message)
{
    if (errorMessage) {
        *errorMessage = message;
    }
}

QString trimmedOrEmpty(const QString &value)
{
    return value.trimmed();
}

} // namespace

QString CableLabelCodec::encodePayload(const CableRecord &record)
{
    return encodePayload(record.code, record.startPoint, record.endPoint);
}

QString CableLabelCodec::encodePayload(const QString &code, const QString &startPoint, const QString &endPoint)
{
    const auto encodeField = [](const QString &value) {
        return QString::fromLatin1(QUrl::toPercentEncoding(value.trimmed()));
    };

    return QStringLiteral("CABLE1|%1|%2|%3").arg(encodeField(code), encodeField(startPoint), encodeField(endPoint));
}

bool CableLabelCodec::decodePayload(const QString &text, CableQrScanData *data, QString *errorMessage)
{
    if (!data) {
        setError(errorMessage, QStringLiteral("无效的扫码输出。"));
        return false;
    }

    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        setError(errorMessage, QStringLiteral("扫码内容为空。"));
        return false;
    }

    if (trimmed.startsWith(QStringLiteral("CABLE1|"))) {
        const QStringList parts = trimmed.split(QLatin1Char('|'));
        if (parts.size() != 4) {
            setError(errorMessage, QStringLiteral("二维码内容格式不正确。"));
            return false;
        }

        data->code = QUrl::fromPercentEncoding(parts.at(1).toLatin1()).trimmed();
        data->startPoint = QUrl::fromPercentEncoding(parts.at(2).toLatin1()).trimmed();
        data->endPoint = QUrl::fromPercentEncoding(parts.at(3).toLatin1()).trimmed();
        if (data->code.isEmpty()) {
            setError(errorMessage, QStringLiteral("二维码中缺少编号。"));
            return false;
        }
        return true;
    }

    if (trimmed.startsWith(QLatin1Char('{'))) {
        setError(errorMessage, QStringLiteral("二维码内容格式不正确。"));
        return false;
    }

    if (trimmed.contains(QLatin1Char('|'))) {
        setError(errorMessage, QStringLiteral("二维码内容格式不正确。"));
        return false;
    }

    data->code = trimmed;
    data->startPoint.clear();
    data->endPoint.clear();
    if (data->code.isEmpty()) {
        setError(errorMessage, QStringLiteral("二维码中缺少编号。"));
        return false;
    }
    return true;
}

QString CableLabelCodec::displayText(const QString &code, const QString &startPoint, const QString &endPoint)
{
    return QStringLiteral("%1\n%2 -> %3").arg(trimmedOrEmpty(code), trimmedOrEmpty(startPoint), trimmedOrEmpty(endPoint));
}
