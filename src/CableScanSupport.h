#pragma once

#include "CableLabelCodec.h"
#include "DatabaseManager.h"

#include <QString>

enum class CableScanMode
{
    Borrow,
    Return
};

struct CableScanResolvedCable
{
    CableQrScanData scanData;
    CableRecord record;
};

class CableScanSupport
{
public:
    static bool resolveScanText(const DatabaseManager &db,
                                const QString &text,
                                CableScanResolvedCable *resolved,
                                QString *errorMessage = nullptr);
    static bool canQueueForMode(CableScanMode mode, const CableRecord &record, QString *errorMessage = nullptr);
    static QString modeTitle(CableScanMode mode);
    static QString cacheTitle(CableScanMode mode);
    static QString actionText(CableScanMode mode);
    static QString scanPrompt(CableScanMode mode);
};
