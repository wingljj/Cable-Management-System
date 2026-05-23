#include "CableLabelPrinter.h"

#include "CableLabelCodec.h"
#include "DatabaseManager.h"

#include "qrcodegen.hpp"

#include <QDialog>
#include <QFont>
#include <QPainter>
#include <QPageLayout>
#include <QPrinter>
#include <QPrinterInfo>
#include <QPrintDialog>
#include <QRect>
#include <QRectF>
#include <QWidget>

using qrcodegen::QrCode;

namespace {

QImage renderOnePreview(const QString &code, const QString &startPoint, const QString &endPoint, const QSize &size)
{
    const QString payload = CableLabelCodec::encodePayload(code, startPoint, endPoint);
    const QrCode qr = QrCode::encodeText(payload.toUtf8().constData(), QrCode::Ecc::MEDIUM);

    QImage image(size, QImage::Format_RGB32);
    image.fill(Qt::white);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const int margin = 24;
    const int qrSide = qMin(size.height() - margin * 2, size.width() / 3 - margin);
    const QRect qrRect(margin, margin, qrSide, qrSide);
    const int moduleCount = qr.getSize();
    const double scale = static_cast<double>(qrRect.width()) / static_cast<double>(moduleCount);

    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black);
    for (int y = 0; y < moduleCount; ++y) {
        for (int x = 0; x < moduleCount; ++x) {
            if (qr.getModule(x, y)) {
                const QRectF cell(qrRect.left() + x * scale, qrRect.top() + y * scale, scale + 0.2, scale + 0.2);
                painter.drawRect(cell);
            }
        }
    }

    painter.setPen(Qt::black);
    const QRect textArea(qrRect.right() + 28, margin, size.width() - qrRect.right() - margin - 28, size.height() - margin * 2);

    QFont codeFont(QStringLiteral("Microsoft YaHei UI"));
    codeFont.setPointSize(30);
    codeFont.setBold(true);
    painter.setFont(codeFont);
    painter.drawText(textArea, Qt::AlignTop | Qt::AlignLeft, code);

    QFont bodyFont(QStringLiteral("Microsoft YaHei UI"));
    bodyFont.setPointSize(22);
    bodyFont.setBold(true);
    painter.setFont(bodyFont);
    painter.drawText(textArea.adjusted(0, 72, 0, 0), Qt::AlignTop | Qt::AlignLeft,
                     QStringLiteral("终端：%1\n始端：%2").arg(endPoint, startPoint));

    return image;
}

} // namespace

QImage CableLabelPrinter::renderPreview(const QString &code, const QString &startPoint, const QString &endPoint, const QSize &size)
{
    return renderOnePreview(code, startPoint, endPoint, size);
}

QImage CableLabelPrinter::renderPreview(const CableRecord &record, const QSize &size)
{
    return renderOnePreview(record.code, record.startPoint, record.endPoint, size);
}

bool CableLabelPrinter::printLabels(const QList<CableRecord> &records, QWidget *parent, QString *errorMessage)
{
    if (records.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("请先选择要打印的电缆。");
        }
        return false;
    }

    if (QPrinterInfo::availablePrinters().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("系统未找到可用打印机，请先安装并连接 ZT210 打印机驱动。");
        }
        return false;
    }

    QPrinter printer(QPrinter::HighResolution);
    printer.setDocName(QStringLiteral("CableLabels"));
    printer.setFullPage(true);
    printer.setPageMargins(QMarginsF(0, 0, 0, 0), QPageLayout::Millimeter);

    QPrintDialog dialog(&printer, parent);
    dialog.setWindowTitle(QStringLiteral("打印电缆标签"));
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    QPainter painter(&printer);
    if (!painter.isActive()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法启动打印设备。");
        }
        return false;
    }

    const QRectF page = printer.pageRect(QPrinter::DevicePixel);
    for (int i = 0; i < records.size(); ++i) {
        if (i > 0) {
            printer.newPage();
        }

        const QImage preview = renderPreview(records.at(i), page.size().toSize());
        painter.drawImage(page, preview);
    }

    return true;
}
