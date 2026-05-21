#include "CableScanSupport.h"

#include "CableLabelCodec.h"

namespace {

void setError(QString *errorMessage, const QString &message)
{
    if (errorMessage) {
        *errorMessage = message;
    }
}

} // namespace

bool CableScanSupport::resolveScanText(const DatabaseManager &db,
                                       const QString &text,
                                       CableScanResolvedCable *resolved,
                                       QString *errorMessage)
{
    if (!resolved) {
        setError(errorMessage, QStringLiteral("无效的扫码结果。"));
        return false;
    }

    CableQrScanData scanData;
    QString decodeError;
    if (!CableLabelCodec::decodePayload(text, &scanData, &decodeError)) {
        setError(errorMessage, decodeError);
        return false;
    }

    const CableRecord record = db.cableByCode(scanData.code);
    if (record.id <= 0) {
        setError(errorMessage, QStringLiteral("未找到编号为 %1 的电缆。").arg(scanData.code));
        return false;
    }

    resolved->scanData = scanData;
    resolved->record = record;
    return true;
}

bool CableScanSupport::canQueueForMode(CableScanMode mode, const CableRecord &record, QString *errorMessage)
{
    if (record.id <= 0) {
        setError(errorMessage, QStringLiteral("无效的电缆记录。"));
        return false;
    }

    if (mode == CableScanMode::Borrow) {
        if (record.status != QStringLiteral("在库")) {
            setError(errorMessage, QStringLiteral("只有“在库”的电缆可以借出。当前状态：%1").arg(record.status));
            return false;
        }
        return true;
    }

    if (record.status != QStringLiteral("借出")) {
        setError(errorMessage, QStringLiteral("只有“借出”的电缆可以归还。当前状态：%1").arg(record.status));
        return false;
    }
    return true;
}

QString CableScanSupport::modeTitle(CableScanMode mode)
{
    return mode == CableScanMode::Borrow ? QStringLiteral("扫码借出") : QStringLiteral("扫码归还");
}

QString CableScanSupport::cacheTitle(CableScanMode mode)
{
    return mode == CableScanMode::Borrow ? QStringLiteral("借出缓存") : QStringLiteral("归还缓存");
}

QString CableScanSupport::actionText(CableScanMode mode)
{
    return mode == CableScanMode::Borrow ? QStringLiteral("批量借出") : QStringLiteral("批量归还");
}

QString CableScanSupport::scanPrompt(CableScanMode mode)
{
    return mode == CableScanMode::Borrow ? QStringLiteral("扫描电缆后回车，确认后加入借出缓存。")
                                         : QStringLiteral("扫描电缆后回车，确认后加入归还缓存。");
}
