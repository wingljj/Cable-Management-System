#pragma once

#include <QImage>
#include <QList>
#include <QString>
#include <QSize>

struct CableRecord;

class QWidget;

class CableLabelPrinter
{
public:
    static QImage renderPreview(const QString &code,
                                const QString &startPoint,
                                const QString &endPoint,
                                const QSize &size = QSize(800, 480));

    static QImage renderPreview(const CableRecord &record, const QSize &size = QSize(800, 480));

    static bool printLabels(const QList<CableRecord> &records,
                            QWidget *parent = nullptr,
                            QString *errorMessage = nullptr);
};
