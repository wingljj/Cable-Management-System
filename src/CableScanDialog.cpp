#include "CableScanDialog.h"

#include "CableLabelCodec.h"
#include "DatabaseManager.h"

#include <QBoxLayout>
#include <QAbstractItemView>
#include <QDateEdit>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QModelIndex>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSqlQueryModel>
#include <QTableView>

namespace {

void configureTable(QTableView *table)
{
    table->setAlternatingRowColors(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setHighlightSections(false);
    table->verticalHeader()->setVisible(false);
}

} // namespace

CableScanDialog::CableScanDialog(DatabaseManager *db, CableScanMode mode, QWidget *parent)
    : QDialog(parent)
    , m_db(db)
    , m_mode(mode)
{
    setModal(true);
    buildUi();
    refreshSearchResults();
    m_scanEdit->setFocus();
}

void CableScanDialog::buildUi()
{
    setWindowTitle(CableScanSupport::modeTitle(m_mode));
    resize(1180, 760);
    setMinimumSize(1040, 700);

    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(14, 14, 14, 14);
    root->setSpacing(14);

    auto *left = new QVBoxLayout();

    auto *scanBox = new QGroupBox(CableScanSupport::modeTitle(m_mode), this);
    auto *scanLayout = new QVBoxLayout(scanBox);
    m_scanEdit = new QLineEdit(scanBox);
    m_scanEdit->setEchoMode(QLineEdit::NoEcho);
    m_scanEdit->setPlaceholderText(QStringLiteral("扫描枪回车后自动确认"));
    scanLayout->addWidget(new QLabel(CableScanSupport::scanPrompt(m_mode), scanBox));
    scanLayout->addWidget(m_scanEdit);

    auto *searchBox = new QGroupBox(QStringLiteral("临时手动搜索"), this);
    auto *searchLayout = new QVBoxLayout(searchBox);
    auto *searchBar = new QHBoxLayout();
    m_searchEdit = new QLineEdit(searchBox);
    m_searchEdit->setPlaceholderText(QStringLiteral("编号 / 始端 / 终端"));
    auto *searchButton = new QPushButton(QStringLiteral("搜索"), searchBox);
    auto *addButton = new QPushButton(QStringLiteral("加入缓存"), searchBox);
    searchBar->addWidget(m_searchEdit, 1);
    searchBar->addWidget(searchButton);
    searchBar->addWidget(addButton);

    m_searchTable = new QTableView(searchBox);
    configureTable(m_searchTable);
    searchLayout->addLayout(searchBar);
    searchLayout->addWidget(m_searchTable, 1);

    left->addWidget(scanBox);
    left->addWidget(searchBox, 1);

    m_cacheBox = new QGroupBox(QStringLiteral("%1（0 根）").arg(CableScanSupport::cacheTitle(m_mode)), this);
    m_cacheBox->setMaximumWidth(420);
    auto *cacheLayout = new QVBoxLayout(m_cacheBox);
    m_cacheList = new QListWidget(m_cacheBox);
    m_cacheList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    cacheLayout->addWidget(m_cacheList, 1);

    auto *cacheButtons = new QHBoxLayout();
    auto *removeButton = new QPushButton(QStringLiteral("移除选中"), m_cacheBox);
    auto *clearButton = new QPushButton(QStringLiteral("清空缓存"), m_cacheBox);
    cacheButtons->addWidget(removeButton);
    cacheButtons->addWidget(clearButton);
    cacheLayout->addLayout(cacheButtons);

    auto *form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight);
    if (m_mode == CableScanMode::Borrow) {
        m_borrowerEdit = new QLineEdit(m_cacheBox);
        m_departmentEdit = new QLineEdit(m_cacheBox);
        m_borrowDateEdit = new QDateEdit(QDate::currentDate(), m_cacheBox);
        m_borrowDateEdit->setCalendarPopup(true);
        m_borrowDateEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
        m_expectedReturnEdit = new QDateEdit(QDate::currentDate().addDays(7), m_cacheBox);
        m_expectedReturnEdit->setCalendarPopup(true);
        m_expectedReturnEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
        m_remarkEdit = new QPlainTextEdit(m_cacheBox);
        m_remarkEdit->setFixedHeight(82);

        form->addRow(QStringLiteral("借用人"), m_borrowerEdit);
        form->addRow(QStringLiteral("部门"), m_departmentEdit);
        form->addRow(QStringLiteral("借出日期"), m_borrowDateEdit);
        form->addRow(QStringLiteral("预计归还"), m_expectedReturnEdit);
        form->addRow(QStringLiteral("备注"), m_remarkEdit);
        m_actionButton = new QPushButton(CableScanSupport::actionText(m_mode), m_cacheBox);
        form->addRow(m_actionButton);
    } else {
        m_actualReturnEdit = new QDateEdit(QDate::currentDate(), m_cacheBox);
        m_actualReturnEdit->setCalendarPopup(true);
        m_actualReturnEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
        m_remarkEdit = new QPlainTextEdit(m_cacheBox);
        m_remarkEdit->setFixedHeight(82);

        form->addRow(QStringLiteral("实际归还"), m_actualReturnEdit);
        form->addRow(QStringLiteral("备注"), m_remarkEdit);
        m_actionButton = new QPushButton(CableScanSupport::actionText(m_mode), m_cacheBox);
        form->addRow(m_actionButton);
    }
    cacheLayout->addLayout(form);

    root->addLayout(left, 1);
    root->addWidget(m_cacheBox);

    connect(m_scanEdit, &QLineEdit::returnPressed, this, &CableScanDialog::handleScanReturn);
    connect(searchButton, &QPushButton::clicked, this, &CableScanDialog::searchCables);
    connect(m_searchEdit, &QLineEdit::returnPressed, this, &CableScanDialog::searchCables);
    connect(addButton, &QPushButton::clicked, this, &CableScanDialog::addSelectedSearchResult);
    connect(m_searchTable, &QTableView::doubleClicked, this, [this](const QModelIndex &index) {
        if (index.isValid()) {
            addSelectedSearchResultAtRow(index.row());
        }
    });
    connect(removeButton, &QPushButton::clicked, this, &CableScanDialog::removeSelectedCacheItems);
    connect(clearButton, &QPushButton::clicked, this, &CableScanDialog::clearCache);
    connect(m_actionButton, &QPushButton::clicked, this, [this]() {
        if (m_mode == CableScanMode::Borrow) {
            submitBorrow();
        } else {
            submitReturn();
        }
    });
}

void CableScanDialog::handleScanReturn()
{
    const QString text = m_scanEdit->text().trimmed();
    if (text.isEmpty()) {
        return;
    }

    CableScanResolvedCable resolved;
    QString error;
    if (!CableScanSupport::resolveScanText(*m_db, text, &resolved, &error)) {
        showError(error);
        m_scanEdit->clear();
        return;
    }

    if (!CableScanSupport::canQueueForMode(m_mode, resolved.record, &error)) {
        showError(error);
        m_scanEdit->clear();
        return;
    }

    const QString confirmText = QStringLiteral("%1\n\n当前状态：%2\n\n确认加入%3吗？")
                                    .arg(CableLabelCodec::displayText(resolved.scanData.code,
                                                                       resolved.scanData.startPoint,
                                                                       resolved.scanData.endPoint),
                                         resolved.record.status,
                                         CableScanSupport::cacheTitle(m_mode));
    if (QMessageBox::question(this, CableScanSupport::modeTitle(m_mode), confirmText) == QMessageBox::Yes) {
        addCableToCache(resolved.record);
    }

    m_scanEdit->clear();
    m_scanEdit->setFocus();
}

void CableScanDialog::searchCables()
{
    refreshSearchResults();
}

void CableScanDialog::addSelectedSearchResult()
{
    if (!m_searchTable || !m_searchTable->model() || !m_searchTable->selectionModel()) {
        return;
    }

    const QModelIndexList rows = m_searchTable->selectionModel()->selectedRows();
    if (rows.isEmpty()) {
        showError(QStringLiteral("请先选择要加入缓存的电缆。"));
        return;
    }

    for (const QModelIndex &rowIndex : rows) {
        addSelectedSearchResultAtRow(rowIndex.row());
    }
}

void CableScanDialog::addSelectedSearchResultAtRow(int row)
{
    if (!m_searchTable || !m_searchTable->model()) {
        return;
    }

    const int id = m_searchTable->model()->index(row, 0).data().toInt();
    if (id <= 0) {
        return;
    }

    const CableRecord record = m_db->cable(id);
    if (record.id > 0) {
        confirmAndQueueCable(record, QStringLiteral("手动搜索"));
    }
}

void CableScanDialog::removeSelectedCacheItems()
{
    const QList<QListWidgetItem *> items = m_cacheList->selectedItems();
    for (QListWidgetItem *item : items) {
        m_cachedIds.remove(item->data(Qt::UserRole).toInt());
        delete item;
    }
    updateCacheTitle();
}

void CableScanDialog::clearCache()
{
    m_cachedIds.clear();
    m_cacheList->clear();
    updateCacheTitle();
}

void CableScanDialog::submitBorrow()
{
    if (m_cachedIds.isEmpty()) {
        showError(QStringLiteral("请先把电缆加入缓存。"));
        return;
    }
    if (!m_borrowerEdit || m_borrowerEdit->text().trimmed().isEmpty()) {
        showError(QStringLiteral("请填写借用人。"));
        return;
    }

    CableBorrowRecord record;
    record.borrower = m_borrowerEdit->text();
    record.department = m_departmentEdit->text();
    record.borrowDate = m_borrowDateEdit->date();
    record.expectedReturnDate = m_expectedReturnEdit->date();
    record.remark = m_remarkEdit->toPlainText();

    if (!m_db->borrowCables(cachedCableIds(), record)) {
        showError(m_db->lastError());
        return;
    }

    QMessageBox::information(this, CableScanSupport::modeTitle(m_mode), QStringLiteral("已完成批量借出。"));
    clearCache();
    resetActionForm();
}

void CableScanDialog::submitReturn()
{
    if (m_cachedIds.isEmpty()) {
        showError(QStringLiteral("请先把电缆加入缓存。"));
        return;
    }

    if (!m_db->returnCables(cachedCableIds(), m_actualReturnEdit->date(), m_remarkEdit->toPlainText())) {
        showError(m_db->lastError());
        return;
    }

    QMessageBox::information(this, CableScanSupport::modeTitle(m_mode), QStringLiteral("已完成批量归还。"));
    clearCache();
    resetActionForm();
}

void CableScanDialog::refreshSearchResults()
{
    replaceModel(m_searchModel, m_db->createCableModel(m_searchEdit->text(), QString(), this), m_searchTable);
    m_searchTable->hideColumn(0);
    m_searchTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

void CableScanDialog::addCableToCache(const CableRecord &record)
{
    if (record.id <= 0) {
        return;
    }
    if (m_cachedIds.contains(record.id)) {
        QMessageBox::information(this,
                                 CableScanSupport::modeTitle(m_mode),
                                 QStringLiteral("电缆 %1 已在缓存中。").arg(record.code));
        return;
    }

    auto *item = new QListWidgetItem(QStringLiteral("%1    %2 -> %3    [%4]")
                                         .arg(record.code, record.startPoint, record.endPoint, record.status),
                                     m_cacheList);
    item->setData(Qt::UserRole, record.id);
    m_cachedIds.insert(record.id);
    updateCacheTitle();
}

void CableScanDialog::confirmAndQueueCable(const CableRecord &record, const QString &originText)
{
    if (record.id <= 0) {
        return;
    }

    QString error;
    if (!CableScanSupport::canQueueForMode(m_mode, record, &error)) {
        showError(error);
        return;
    }

    const QString text = QStringLiteral("%1\n\n来源：%2\n当前状态：%3\n\n确认加入%4吗？")
                             .arg(CableLabelCodec::displayText(record.code, record.startPoint, record.endPoint),
                                  originText,
                                  record.status,
                                  CableScanSupport::cacheTitle(m_mode));
    if (QMessageBox::question(this, CableScanSupport::modeTitle(m_mode), text) == QMessageBox::Yes) {
        addCableToCache(record);
    }
}

QList<int> CableScanDialog::cachedCableIds() const
{
    QList<int> ids;
    for (int i = 0; i < m_cacheList->count(); ++i) {
        ids.append(m_cacheList->item(i)->data(Qt::UserRole).toInt());
    }
    return ids;
}

void CableScanDialog::updateCacheTitle()
{
    if (m_cacheBox) {
        m_cacheBox->setTitle(QStringLiteral("%1（%2 根）").arg(CableScanSupport::cacheTitle(m_mode)).arg(m_cacheList->count()));
    }
}

void CableScanDialog::resetActionForm()
{
    if (m_mode == CableScanMode::Borrow) {
        m_borrowerEdit->clear();
        m_departmentEdit->clear();
        m_borrowDateEdit->setDate(QDate::currentDate());
        m_expectedReturnEdit->setDate(QDate::currentDate().addDays(7));
    } else {
        m_actualReturnEdit->setDate(QDate::currentDate());
    }
    m_remarkEdit->clear();
}

void CableScanDialog::showError(const QString &message)
{
    QMessageBox::warning(this, CableScanSupport::modeTitle(m_mode), message);
}

void CableScanDialog::replaceModel(QPointer<QSqlQueryModel> &target, QSqlQueryModel *model, QTableView *table)
{
    if (target) {
        target->deleteLater();
    }
    target = model;
    table->setModel(model);
    table->resizeColumnsToContents();
}
