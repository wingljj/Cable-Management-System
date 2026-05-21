#include "MainWindow.h"

#include "CableImporter.h"
#include "CableLabelCodec.h"
#include "CableLabelPrinter.h"

#include <QApplication>
#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSqlQueryModel>
#include <QStatusBar>
#include <QTableView>
#include <QTabWidget>
#include <QTextEdit>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    if (!m_db.open()) {
        QMessageBox::critical(this, QStringLiteral("启动失败"), m_db.lastError());
    }

    buildUi();
    connect(&m_db, &DatabaseManager::dataChanged, this, &MainWindow::refreshAll);
    refreshAll();
}

void MainWindow::buildUi()
{
    setWindowTitle(QStringLiteral("设备工装管理系统"));
    resize(1280, 760);
    setMinimumSize(1080, 680);

    qApp->setStyleSheet(QStringLiteral(
        "QMainWindow { background: #f4f6f8; }"
        "QTabWidget::pane { border: 1px solid #d7dde5; background: white; }"
        "QTabBar::tab { padding: 9px 18px; margin-right: 2px; }"
        "QTabBar::tab:selected { background: white; border: 1px solid #d7dde5; border-bottom-color: white; }"
        "QGroupBox { border: 1px solid #d7dde5; border-radius: 6px; margin-top: 12px; padding: 12px; background: white; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; color: #34495e; }"
        "QLineEdit, QComboBox, QDateEdit, QPlainTextEdit, QTextEdit { border: 1px solid #c8d0da; border-radius: 4px; padding: 5px; background: white; }"
        "QPushButton { background: #245b9f; color: white; border: none; border-radius: 4px; padding: 7px 14px; }"
        "QPushButton:hover { background: #1d4d87; }"
        "QPushButton:pressed { background: #173e6c; }"
        "QTableView { border: 1px solid #d7dde5; selection-background-color: #d8e8ff; gridline-color: #edf1f5; }"
        "QHeaderView::section { background: #eef3f8; border: 0; border-right: 1px solid #d7dde5; padding: 7px; font-weight: 600; }"));

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(buildEquipmentTab(), QStringLiteral("设备台账"));
    m_tabs->addTab(buildBorrowTab(), QStringLiteral("借入借出"));
    m_tabs->addTab(buildRepairTab(), QStringLiteral("报修管理"));
    m_tabs->addTab(buildCableTab(), QStringLiteral("电缆管理"));
    m_tabs->addTab(buildStatsTab(), QStringLiteral("统计分析"));
    setCentralWidget(m_tabs);
}

QWidget *MainWindow::buildEquipmentTab()
{
    auto *page = new QWidget(this);
    auto *root = new QHBoxLayout(page);
    root->setContentsMargins(14, 14, 14, 14);
    root->setSpacing(14);

    auto *left = new QVBoxLayout();
    auto *filterBox = new QGroupBox(QStringLiteral("查询"));
    auto *filterLayout = new QGridLayout(filterBox);
    m_equipmentKeyword = new QLineEdit(filterBox);
    m_equipmentKeyword->setPlaceholderText(QStringLiteral("编号 / 名称 / 型号 / 责任人"));
    m_equipmentCategoryFilter = new QComboBox(filterBox);
    m_equipmentStatusFilter = new QComboBox(filterBox);
    auto *searchButton = new QPushButton(QStringLiteral("查询"), filterBox);
    auto *resetButton = new QPushButton(QStringLiteral("重置"), filterBox);

    filterLayout->addWidget(new QLabel(QStringLiteral("关键字")), 0, 0);
    filterLayout->addWidget(m_equipmentKeyword, 0, 1);
    filterLayout->addWidget(new QLabel(QStringLiteral("类别")), 0, 2);
    filterLayout->addWidget(m_equipmentCategoryFilter, 0, 3);
    filterLayout->addWidget(new QLabel(QStringLiteral("状态")), 0, 4);
    filterLayout->addWidget(m_equipmentStatusFilter, 0, 5);
    filterLayout->addWidget(searchButton, 0, 6);
    filterLayout->addWidget(resetButton, 0, 7);
    filterLayout->setColumnStretch(1, 2);
    filterLayout->setColumnStretch(3, 1);

    m_equipmentTable = new QTableView(page);
    setupTable(m_equipmentTable);
    left->addWidget(filterBox);
    left->addWidget(m_equipmentTable, 1);

    auto *formBox = new QGroupBox(QStringLiteral("设备/工装信息"));
    formBox->setMaximumWidth(380);
    auto *formLayout = new QFormLayout(formBox);
    formLayout->setLabelAlignment(Qt::AlignRight);
    m_codeEdit = new QLineEdit(formBox);
    m_nameEdit = new QLineEdit(formBox);
    m_categoryEdit = new QLineEdit(formBox);
    m_categoryEdit->setPlaceholderText(QStringLiteral("如：检测设备、工装夹具"));
    m_modelEdit = new QLineEdit(formBox);
    m_locationEdit = new QLineEdit(formBox);
    m_ownerEdit = new QLineEdit(formBox);
    m_statusEdit = new QComboBox(formBox);
    m_statusEdit->addItems(m_db.equipmentStatuses());
    m_purchaseDateEdit = new QDateEdit(QDate::currentDate(), formBox);
    m_purchaseDateEdit->setCalendarPopup(true);
    m_purchaseDateEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_remarkEdit = new QPlainTextEdit(formBox);
    m_remarkEdit->setFixedHeight(78);

    formLayout->addRow(QStringLiteral("编号"), m_codeEdit);
    formLayout->addRow(QStringLiteral("名称"), m_nameEdit);
    formLayout->addRow(QStringLiteral("类别"), m_categoryEdit);
    formLayout->addRow(QStringLiteral("型号"), m_modelEdit);
    formLayout->addRow(QStringLiteral("位置"), m_locationEdit);
    formLayout->addRow(QStringLiteral("责任人"), m_ownerEdit);
    formLayout->addRow(QStringLiteral("状态"), m_statusEdit);
    formLayout->addRow(QStringLiteral("购置日期"), m_purchaseDateEdit);
    formLayout->addRow(QStringLiteral("备注"), m_remarkEdit);

    auto *buttonRow = new QHBoxLayout();
    auto *newButton = new QPushButton(QStringLiteral("新增"), formBox);
    m_saveEquipmentButton = new QPushButton(QStringLiteral("保存"), formBox);
    auto *deleteButton = new QPushButton(QStringLiteral("删除"), formBox);
    buttonRow->addWidget(newButton);
    buttonRow->addWidget(m_saveEquipmentButton);
    buttonRow->addWidget(deleteButton);
    formLayout->addRow(buttonRow);

    root->addLayout(left, 1);
    root->addWidget(formBox);

    connect(searchButton, &QPushButton::clicked, this, &MainWindow::refreshEquipment);
    connect(m_equipmentKeyword, &QLineEdit::returnPressed, this, &MainWindow::refreshEquipment);
    connect(resetButton, &QPushButton::clicked, this, [this]() {
        m_equipmentKeyword->clear();
        m_equipmentCategoryFilter->setCurrentIndex(0);
        m_equipmentStatusFilter->setCurrentIndex(0);
        refreshEquipment();
    });
    connect(newButton, &QPushButton::clicked, this, &MainWindow::newEquipment);
    connect(m_saveEquipmentButton, &QPushButton::clicked, this, &MainWindow::saveEquipmentFromForm);
    connect(deleteButton, &QPushButton::clicked, this, &MainWindow::deleteSelectedEquipment);
    connect(m_equipmentTable, &QTableView::doubleClicked, this, &MainWindow::editSelectedEquipment);
    connect(m_equipmentTable, &QTableView::clicked, this, [this]() {
        const int id = selectedId(m_equipmentTable);
        if (id > 0) {
            loadEquipmentToForm(id);
        }
    });

    return page;
}

QWidget *MainWindow::buildBorrowTab()
{
    auto *page = new QWidget(this);
    auto *root = new QHBoxLayout(page);
    root->setContentsMargins(14, 14, 14, 14);
    root->setSpacing(14);

    auto *left = new QVBoxLayout();
    auto *filterBox = new QGroupBox(QStringLiteral("借出记录查询"));
    auto *filterLayout = new QGridLayout(filterBox);
    m_borrowKeyword = new QLineEdit(filterBox);
    m_borrowKeyword->setPlaceholderText(QStringLiteral("设备 / 借用人 / 部门"));
    m_borrowStatusFilter = new QComboBox(filterBox);
    m_borrowStatusFilter->addItem(QStringLiteral("全部"));
    m_borrowStatusFilter->addItems(m_db.borrowStatuses());
    auto *searchButton = new QPushButton(QStringLiteral("查询"), filterBox);
    auto *returnButton = new QPushButton(QStringLiteral("归还选中"), filterBox);
    filterLayout->addWidget(new QLabel(QStringLiteral("关键字")), 0, 0);
    filterLayout->addWidget(m_borrowKeyword, 0, 1);
    filterLayout->addWidget(new QLabel(QStringLiteral("状态")), 0, 2);
    filterLayout->addWidget(m_borrowStatusFilter, 0, 3);
    filterLayout->addWidget(searchButton, 0, 4);
    filterLayout->addWidget(returnButton, 0, 5);
    filterLayout->setColumnStretch(1, 1);

    m_borrowTable = new QTableView(page);
    setupTable(m_borrowTable);
    left->addWidget(filterBox);
    left->addWidget(m_borrowTable, 1);

    auto *formBox = new QGroupBox(QStringLiteral("登记借出"));
    formBox->setMaximumWidth(390);
    auto *formLayout = new QFormLayout(formBox);
    formLayout->setLabelAlignment(Qt::AlignRight);
    m_borrowEquipmentCombo = new QComboBox(formBox);
    m_borrowEquipmentCombo->setMinimumWidth(260);
    m_borrowerEdit = new QLineEdit(formBox);
    m_departmentEdit = new QLineEdit(formBox);
    m_borrowDateEdit = new QDateEdit(QDate::currentDate(), formBox);
    m_borrowDateEdit->setCalendarPopup(true);
    m_borrowDateEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_expectedReturnEdit = new QDateEdit(QDate::currentDate().addDays(7), formBox);
    m_expectedReturnEdit->setCalendarPopup(true);
    m_expectedReturnEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_borrowRemarkEdit = new QPlainTextEdit(formBox);
    m_borrowRemarkEdit->setFixedHeight(92);
    auto *createButton = new QPushButton(QStringLiteral("确认借出"), formBox);

    formLayout->addRow(QStringLiteral("设备/工装"), m_borrowEquipmentCombo);
    formLayout->addRow(QStringLiteral("借用人"), m_borrowerEdit);
    formLayout->addRow(QStringLiteral("部门"), m_departmentEdit);
    formLayout->addRow(QStringLiteral("借出日期"), m_borrowDateEdit);
    formLayout->addRow(QStringLiteral("预计归还"), m_expectedReturnEdit);
    formLayout->addRow(QStringLiteral("备注"), m_borrowRemarkEdit);
    formLayout->addRow(createButton);

    root->addLayout(left, 1);
    root->addWidget(formBox);

    connect(searchButton, &QPushButton::clicked, this, &MainWindow::refreshBorrow);
    connect(m_borrowKeyword, &QLineEdit::returnPressed, this, &MainWindow::refreshBorrow);
    connect(returnButton, &QPushButton::clicked, this, &MainWindow::returnSelectedBorrow);
    connect(createButton, &QPushButton::clicked, this, &MainWindow::createBorrow);

    return page;
}

QWidget *MainWindow::buildRepairTab()
{
    auto *page = new QWidget(this);
    auto *root = new QHBoxLayout(page);
    root->setContentsMargins(14, 14, 14, 14);
    root->setSpacing(14);

    auto *left = new QVBoxLayout();
    auto *filterBox = new QGroupBox(QStringLiteral("报修查询"));
    auto *filterLayout = new QGridLayout(filterBox);
    m_repairKeyword = new QLineEdit(filterBox);
    m_repairKeyword->setPlaceholderText(QStringLiteral("设备 / 报修人 / 故障"));
    m_repairStatusFilter = new QComboBox(filterBox);
    m_repairStatusFilter->addItem(QStringLiteral("全部"));
    m_repairStatusFilter->addItems(m_db.repairStatuses());
    auto *searchButton = new QPushButton(QStringLiteral("查询"), filterBox);
    auto *updateButton = new QPushButton(QStringLiteral("更新选中"), filterBox);
    filterLayout->addWidget(new QLabel(QStringLiteral("关键字")), 0, 0);
    filterLayout->addWidget(m_repairKeyword, 0, 1);
    filterLayout->addWidget(new QLabel(QStringLiteral("状态")), 0, 2);
    filterLayout->addWidget(m_repairStatusFilter, 0, 3);
    filterLayout->addWidget(searchButton, 0, 4);
    filterLayout->addWidget(updateButton, 0, 5);
    filterLayout->setColumnStretch(1, 1);

    m_repairTable = new QTableView(page);
    setupTable(m_repairTable);
    left->addWidget(filterBox);
    left->addWidget(m_repairTable, 1);

    auto *formBox = new QGroupBox(QStringLiteral("报修 / 处理"));
    formBox->setMaximumWidth(410);
    auto *formLayout = new QFormLayout(formBox);
    formLayout->setLabelAlignment(Qt::AlignRight);
    m_repairEquipmentCombo = new QComboBox(formBox);
    m_reporterEdit = new QLineEdit(formBox);
    m_reportDateEdit = new QDateEdit(QDate::currentDate(), formBox);
    m_reportDateEdit->setCalendarPopup(true);
    m_reportDateEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_issueEdit = new QPlainTextEdit(formBox);
    m_issueEdit->setFixedHeight(76);
    auto *createButton = new QPushButton(QStringLiteral("登记报修"), formBox);
    m_repairStatusEdit = new QComboBox(formBox);
    m_repairStatusEdit->addItems(m_db.repairStatuses());
    m_handlerEdit = new QLineEdit(formBox);
    m_finishedDateEdit = new QDateEdit(QDate::currentDate(), formBox);
    m_finishedDateEdit->setCalendarPopup(true);
    m_finishedDateEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_solutionEdit = new QTextEdit(formBox);
    m_solutionEdit->setFixedHeight(88);

    formLayout->addRow(QStringLiteral("设备/工装"), m_repairEquipmentCombo);
    formLayout->addRow(QStringLiteral("报修人"), m_reporterEdit);
    formLayout->addRow(QStringLiteral("报修日期"), m_reportDateEdit);
    formLayout->addRow(QStringLiteral("故障描述"), m_issueEdit);
    formLayout->addRow(createButton);
    formLayout->addRow(new QLabel(QStringLiteral("处理信息")));
    formLayout->addRow(QStringLiteral("维修状态"), m_repairStatusEdit);
    formLayout->addRow(QStringLiteral("处理人"), m_handlerEdit);
    formLayout->addRow(QStringLiteral("完成日期"), m_finishedDateEdit);
    formLayout->addRow(QStringLiteral("处理结果"), m_solutionEdit);
    formLayout->addRow(updateButton);

    root->addLayout(left, 1);
    root->addWidget(formBox);

    connect(searchButton, &QPushButton::clicked, this, &MainWindow::refreshRepair);
    connect(m_repairKeyword, &QLineEdit::returnPressed, this, &MainWindow::refreshRepair);
    connect(createButton, &QPushButton::clicked, this, &MainWindow::createRepair);
    connect(updateButton, &QPushButton::clicked, this, &MainWindow::updateSelectedRepair);

    return page;
}

QWidget *MainWindow::buildCableTab()
{
    auto *page = new QWidget(this);
    auto *root = new QHBoxLayout(page);
    root->setContentsMargins(14, 14, 14, 14);
    root->setSpacing(14);

    auto *left = new QVBoxLayout();
    auto *ledgerBox = new QGroupBox(QStringLiteral("电缆台账"));
    auto *ledgerLayout = new QVBoxLayout(ledgerBox);

    auto *filterLayout = new QGridLayout();
    m_cableKeyword = new QLineEdit(ledgerBox);
    m_cableKeyword->setPlaceholderText(QStringLiteral("编号 / 始端 / 终端"));
    m_cableStatusFilter = new QComboBox(ledgerBox);
    m_cableStatusFilter->addItem(QStringLiteral("全部"));
    m_cableStatusFilter->addItems(m_db.cableStatuses());
    m_clearCableSearchAfterEnter = new QCheckBox(QStringLiteral("回车后清空"), ledgerBox);
    m_addCableSearchToCache = new QCheckBox(QStringLiteral("回车加入缓存"), ledgerBox);
    auto *searchButton = new QPushButton(QStringLiteral("查询"), ledgerBox);
    auto *resetButton = new QPushButton(QStringLiteral("重置"), ledgerBox);
    auto *importButton = new QPushButton(QStringLiteral("导入 Excel/CSV"), ledgerBox);
    auto *cacheSelectedButton = new QPushButton(QStringLiteral("加入缓存"), ledgerBox);
    auto *printSelectedButton = new QPushButton(QStringLiteral("打印标签"), ledgerBox);

    filterLayout->addWidget(new QLabel(QStringLiteral("关键字")), 0, 0);
    filterLayout->addWidget(m_cableKeyword, 0, 1);
    filterLayout->addWidget(m_clearCableSearchAfterEnter, 0, 2);
    filterLayout->addWidget(m_addCableSearchToCache, 0, 3);
    filterLayout->addWidget(new QLabel(QStringLiteral("状态")), 0, 4);
    filterLayout->addWidget(m_cableStatusFilter, 0, 5);
    filterLayout->addWidget(searchButton, 0, 6);
    filterLayout->addWidget(resetButton, 0, 7);
    filterLayout->addWidget(importButton, 0, 8);
    filterLayout->addWidget(cacheSelectedButton, 0, 9);
    filterLayout->addWidget(printSelectedButton, 0, 10);
    filterLayout->setColumnStretch(1, 2);

    m_cableTable = new QTableView(ledgerBox);
    setupStretchTable(m_cableTable);
    m_cableTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ledgerLayout->addLayout(filterLayout);
    ledgerLayout->addWidget(m_cableTable);
    left->addWidget(ledgerBox, 2);

    auto *recordBox = new QGroupBox(QStringLiteral("电缆借还记录"));
    auto *recordLayout = new QVBoxLayout(recordBox);
    auto *recordFilterLayout = new QGridLayout();
    m_cableBorrowKeyword = new QLineEdit(recordBox);
    m_cableBorrowKeyword->setPlaceholderText(QStringLiteral("电缆 / 借用人 / 部门"));
    m_cableBorrowStatusFilter = new QComboBox(recordBox);
    m_cableBorrowStatusFilter->addItem(QStringLiteral("全部"));
    m_cableBorrowStatusFilter->addItems(m_db.cableBorrowStatuses());
    auto *recordSearchButton = new QPushButton(QStringLiteral("查询"), recordBox);
    recordFilterLayout->addWidget(new QLabel(QStringLiteral("关键字")), 0, 0);
    recordFilterLayout->addWidget(m_cableBorrowKeyword, 0, 1);
    recordFilterLayout->addWidget(new QLabel(QStringLiteral("状态")), 0, 2);
    recordFilterLayout->addWidget(m_cableBorrowStatusFilter, 0, 3);
    recordFilterLayout->addWidget(recordSearchButton, 0, 4);
    recordFilterLayout->setColumnStretch(1, 1);

    m_cableBorrowTable = new QTableView(recordBox);
    setupStretchTable(m_cableBorrowTable);
    recordLayout->addLayout(recordFilterLayout);
    recordLayout->addWidget(m_cableBorrowTable);
    left->addWidget(recordBox, 1);

    m_cableCacheBox = new QGroupBox(QStringLiteral("缓存栏（0 根）"));
    m_cableCacheBox->setMaximumWidth(410);
    auto *cacheLayout = new QVBoxLayout(m_cableCacheBox);
    m_cableCacheList = new QListWidget(m_cableCacheBox);
    m_cableCacheList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    cacheLayout->addWidget(m_cableCacheList, 1);

    auto *cacheButtons = new QHBoxLayout();
    auto *removeCacheButton = new QPushButton(QStringLiteral("移除选中"), m_cableCacheBox);
    auto *clearCacheButton = new QPushButton(QStringLiteral("清空缓存"), m_cableCacheBox);
    cacheButtons->addWidget(removeCacheButton);
    cacheButtons->addWidget(clearCacheButton);
    cacheLayout->addLayout(cacheButtons);

    auto *form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight);
    m_cableBorrowerEdit = new QLineEdit(m_cableCacheBox);
    m_cableDepartmentEdit = new QLineEdit(m_cableCacheBox);
    m_cableBorrowDateEdit = new QDateEdit(QDate::currentDate(), m_cableCacheBox);
    m_cableBorrowDateEdit->setCalendarPopup(true);
    m_cableBorrowDateEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_cableExpectedReturnEdit = new QDateEdit(QDate::currentDate().addDays(7), m_cableCacheBox);
    m_cableExpectedReturnEdit->setCalendarPopup(true);
    m_cableExpectedReturnEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_cableActualReturnEdit = new QDateEdit(QDate::currentDate(), m_cableCacheBox);
    m_cableActualReturnEdit->setCalendarPopup(true);
    m_cableActualReturnEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_cableRemarkEdit = new QPlainTextEdit(m_cableCacheBox);
    m_cableRemarkEdit->setFixedHeight(82);

    form->addRow(QStringLiteral("借用人"), m_cableBorrowerEdit);
    form->addRow(QStringLiteral("部门"), m_cableDepartmentEdit);
    form->addRow(QStringLiteral("借出日期"), m_cableBorrowDateEdit);
    form->addRow(QStringLiteral("预计归还"), m_cableExpectedReturnEdit);
    form->addRow(QStringLiteral("实际归还"), m_cableActualReturnEdit);
    form->addRow(QStringLiteral("备注"), m_cableRemarkEdit);
    cacheLayout->addLayout(form);

    auto *actionButtons = new QHBoxLayout();
    auto *borrowButton = new QPushButton(QStringLiteral("批量借出"), m_cableCacheBox);
    auto *returnButton = new QPushButton(QStringLiteral("批量归还"), m_cableCacheBox);
    actionButtons->addWidget(borrowButton);
    actionButtons->addWidget(returnButton);
    cacheLayout->addLayout(actionButtons);

    root->addLayout(left, 1);
    root->addWidget(m_cableCacheBox);

    connect(searchButton, &QPushButton::clicked, this, &MainWindow::refreshCables);
    connect(resetButton, &QPushButton::clicked, this, [this]() {
        m_cableKeyword->clear();
        m_cableStatusFilter->setCurrentIndex(0);
        refreshCables();
    });
    connect(importButton, &QPushButton::clicked, this, &MainWindow::importCables);
    connect(cacheSelectedButton, &QPushButton::clicked, this, &MainWindow::addSelectedCableToCache);
    connect(printSelectedButton, &QPushButton::clicked, this, &MainWindow::printSelectedCableLabels);
    connect(m_cableKeyword, &QLineEdit::returnPressed, this, &MainWindow::handleCableSearchReturn);
    connect(recordSearchButton, &QPushButton::clicked, this, &MainWindow::refreshCableBorrows);
    connect(m_cableBorrowKeyword, &QLineEdit::returnPressed, this, &MainWindow::refreshCableBorrows);
    connect(removeCacheButton, &QPushButton::clicked, this, &MainWindow::removeSelectedCableFromCache);
    connect(clearCacheButton, &QPushButton::clicked, this, &MainWindow::clearCableCache);
    connect(borrowButton, &QPushButton::clicked, this, &MainWindow::borrowCachedCables);
    connect(returnButton, &QPushButton::clicked, this, &MainWindow::returnCachedCables);
    connect(m_cableTable, &QTableView::doubleClicked, this, &MainWindow::addSelectedCableToCache);

    return page;
}

QWidget *MainWindow::buildStatsTab()
{
    auto *page = new QWidget(this);
    auto *root = new QVBoxLayout(page);
    root->setContentsMargins(14, 14, 14, 14);
    root->setSpacing(14);

    auto *metrics = new QGridLayout();
    m_totalMetric = createMetricLabel(QStringLiteral("设备总数"));
    m_availableMetric = createMetricLabel(QStringLiteral("在库"));
    m_borrowedMetric = createMetricLabel(QStringLiteral("借出"));
    m_repairingMetric = createMetricLabel(QStringLiteral("维修中"));
    m_openRepairMetric = createMetricLabel(QStringLiteral("待处理报修"));
    m_overdueMetric = createMetricLabel(QStringLiteral("逾期未还"));
    metrics->addWidget(m_totalMetric, 0, 0);
    metrics->addWidget(m_availableMetric, 0, 1);
    metrics->addWidget(m_borrowedMetric, 0, 2);
    metrics->addWidget(m_repairingMetric, 0, 3);
    metrics->addWidget(m_openRepairMetric, 0, 4);
    metrics->addWidget(m_overdueMetric, 0, 5);
    metrics->setColumnStretch(0, 1);
    metrics->setColumnStretch(1, 1);
    metrics->setColumnStretch(2, 1);
    metrics->setColumnStretch(3, 1);
    metrics->setColumnStretch(4, 1);
    metrics->setColumnStretch(5, 1);
    root->addLayout(metrics);

    auto *middle = new QHBoxLayout();
    auto *statusBox = new QGroupBox(QStringLiteral("按状态统计"), page);
    m_statusBarsLayout = new QFormLayout(statusBox);
    auto *categoryBox = new QGroupBox(QStringLiteral("按类别统计"), page);
    m_categoryBarsLayout = new QFormLayout(categoryBox);
    middle->addWidget(statusBox);
    middle->addWidget(categoryBox);
    root->addLayout(middle);

    auto *overdueBox = new QGroupBox(QStringLiteral("逾期未还清单"), page);
    auto *overdueLayout = new QVBoxLayout(overdueBox);
    m_overdueTable = new QTableView(overdueBox);
    setupTable(m_overdueTable);
    overdueLayout->addWidget(m_overdueTable);
    root->addWidget(overdueBox, 1);

    return page;
}

void MainWindow::setupTable(QTableView *table)
{
    table->setAlternatingRowColors(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setHighlightSections(false);
    table->verticalHeader()->setVisible(false);
    table->setSortingEnabled(false);
}

void MainWindow::setupStretchTable(QTableView *table)
{
    setupTable(table);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

void MainWindow::refreshAll()
{
    populateFilters();
    refreshEquipment();
    refreshBorrow();
    refreshRepair();
    refreshCables();
    refreshCableBorrows();
    refreshStats();
}

void MainWindow::refreshEquipment()
{
    replaceModel(m_equipmentModel,
                 m_db.createEquipmentModel(m_equipmentKeyword->text(),
                                           m_equipmentCategoryFilter->currentText(),
                                           m_equipmentStatusFilter->currentText(),
                                           this),
                 m_equipmentTable);
    m_equipmentTable->hideColumn(0);
}

void MainWindow::refreshBorrow()
{
    populateEquipmentCombo(m_borrowEquipmentCombo, true);
    replaceModel(m_borrowModel,
                 m_db.createBorrowModel(m_borrowKeyword->text(), m_borrowStatusFilter->currentText(), this),
                 m_borrowTable);
    m_borrowTable->hideColumn(0);
}

void MainWindow::refreshRepair()
{
    populateEquipmentCombo(m_repairEquipmentCombo, false);
    replaceModel(m_repairModel,
                 m_db.createRepairModel(m_repairKeyword->text(), m_repairStatusFilter->currentText(), this),
                 m_repairTable);
    m_repairTable->hideColumn(0);
}

void MainWindow::refreshCables()
{
    replaceModel(m_cableModel,
                 m_db.createCableModel(m_cableKeyword->text(), m_cableStatusFilter->currentText(), this),
                 m_cableTable);
    m_cableTable->hideColumn(0);
    m_cableTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

void MainWindow::refreshCableBorrows()
{
    replaceModel(m_cableBorrowModel,
                 m_db.createCableBorrowModel(m_cableBorrowKeyword->text(),
                                             m_cableBorrowStatusFilter->currentText(),
                                             this),
                 m_cableBorrowTable);
    m_cableBorrowTable->hideColumn(0);
    m_cableBorrowTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

void MainWindow::refreshStats()
{
    const QVariantMap summary = m_db.summary();
    updateMetric(m_totalMetric, QStringLiteral("设备总数"), summary.value(QStringLiteral("total")).toInt());
    updateMetric(m_availableMetric, QStringLiteral("在库"), summary.value(QStringLiteral("available")).toInt());
    updateMetric(m_borrowedMetric, QStringLiteral("借出"), summary.value(QStringLiteral("borrowed")).toInt());
    updateMetric(m_repairingMetric, QStringLiteral("维修中"), summary.value(QStringLiteral("repairing")).toInt());
    updateMetric(m_openRepairMetric, QStringLiteral("待处理报修"), summary.value(QStringLiteral("openRepairs")).toInt());
    updateMetric(m_overdueMetric, QStringLiteral("逾期未还"), summary.value(QStringLiteral("overdue")).toInt());

    const int total = summary.value(QStringLiteral("total")).toInt();
    rebuildBars(m_statusBarsLayout, m_db.countByStatus(), total);
    rebuildBars(m_categoryBarsLayout, m_db.countByCategory(), total);

    replaceModel(m_overdueModel, m_db.createOverdueBorrowModel(this), m_overdueTable);
    m_overdueTable->resizeColumnsToContents();
}

void MainWindow::newEquipment()
{
    clearEquipmentForm();
    m_codeEdit->setFocus();
}

void MainWindow::editSelectedEquipment()
{
    const int id = selectedId(m_equipmentTable);
    if (id > 0) {
        loadEquipmentToForm(id);
    }
}

void MainWindow::deleteSelectedEquipment()
{
    const int id = selectedId(m_equipmentTable);
    if (id <= 0) {
        showError(QStringLiteral("请先选择要删除的设备/工装。"));
        return;
    }

    if (QMessageBox::question(this, QStringLiteral("确认删除"), QStringLiteral("确定删除选中的设备/工装吗？")) != QMessageBox::Yes) {
        return;
    }

    if (!m_db.removeEquipment(id)) {
        showError(m_db.lastError());
        return;
    }
    clearEquipmentForm();
}

void MainWindow::saveEquipmentFromForm()
{
    const EquipmentRecord record = equipmentFormRecord();
    if (record.code.isEmpty() || record.name.isEmpty() || record.category.isEmpty()) {
        showError(QStringLiteral("编号、名称、类别不能为空。"));
        return;
    }

    if (!m_db.saveEquipment(record)) {
        showError(m_db.lastError());
        return;
    }
    clearEquipmentForm();
}

void MainWindow::clearEquipmentForm()
{
    m_currentEquipmentId = -1;
    m_codeEdit->clear();
    m_nameEdit->clear();
    m_categoryEdit->clear();
    m_modelEdit->clear();
    m_locationEdit->clear();
    m_ownerEdit->clear();
    m_statusEdit->setCurrentText(QStringLiteral("在库"));
    m_purchaseDateEdit->setDate(QDate::currentDate());
    m_remarkEdit->clear();
    m_saveEquipmentButton->setText(QStringLiteral("保存"));
}

void MainWindow::createBorrow()
{
    if (m_borrowEquipmentCombo->currentData().toInt() <= 0 || m_borrowerEdit->text().trimmed().isEmpty()) {
        showError(QStringLiteral("请选择可借出的设备，并填写借用人。"));
        return;
    }

    BorrowRecord record;
    record.equipmentId = m_borrowEquipmentCombo->currentData().toInt();
    record.borrower = m_borrowerEdit->text();
    record.department = m_departmentEdit->text();
    record.borrowDate = m_borrowDateEdit->date();
    record.expectedReturnDate = m_expectedReturnEdit->date();
    record.remark = m_borrowRemarkEdit->toPlainText();

    if (!m_db.createBorrow(record)) {
        showError(m_db.lastError());
        return;
    }

    m_borrowerEdit->clear();
    m_departmentEdit->clear();
    m_borrowDateEdit->setDate(QDate::currentDate());
    m_expectedReturnEdit->setDate(QDate::currentDate().addDays(7));
    m_borrowRemarkEdit->clear();
}

void MainWindow::returnSelectedBorrow()
{
    const int id = selectedId(m_borrowTable);
    if (id <= 0) {
        showError(QStringLiteral("请先选择借出记录。"));
        return;
    }

    if (QMessageBox::question(this, QStringLiteral("确认归还"), QStringLiteral("将选中记录登记为已归还？")) != QMessageBox::Yes) {
        return;
    }

    if (!m_db.returnBorrow(id, QDate::currentDate(), QStringLiteral("已归还"))) {
        showError(m_db.lastError());
    }
}

void MainWindow::createRepair()
{
    if (m_repairEquipmentCombo->currentData().toInt() <= 0 || m_reporterEdit->text().trimmed().isEmpty() ||
        m_issueEdit->toPlainText().trimmed().isEmpty()) {
        showError(QStringLiteral("请选择设备，并填写报修人和故障描述。"));
        return;
    }

    RepairRecord record;
    record.equipmentId = m_repairEquipmentCombo->currentData().toInt();
    record.reporter = m_reporterEdit->text();
    record.reportDate = m_reportDateEdit->date();
    record.issue = m_issueEdit->toPlainText();

    if (!m_db.createRepair(record)) {
        showError(m_db.lastError());
        return;
    }

    m_reporterEdit->clear();
    m_reportDateEdit->setDate(QDate::currentDate());
    m_issueEdit->clear();
}

void MainWindow::updateSelectedRepair()
{
    const int id = selectedId(m_repairTable);
    if (id <= 0) {
        showError(QStringLiteral("请先选择报修记录。"));
        return;
    }

    const QDate finishedDate = (m_repairStatusEdit->currentText() == QStringLiteral("已完成") ||
                               m_repairStatusEdit->currentText() == QStringLiteral("无法修复"))
                                  ? m_finishedDateEdit->date()
                                  : QDate();

    if (!m_db.updateRepairStatus(id,
                                 m_repairStatusEdit->currentText(),
                                 m_handlerEdit->text(),
                                 finishedDate,
                                 m_solutionEdit->toPlainText())) {
        showError(m_db.lastError());
    }
}

void MainWindow::importCables()
{
    const QString path = QFileDialog::getOpenFileName(this,
                                                      QStringLiteral("导入电缆台账"),
                                                      QString(),
                                                      QStringLiteral("Excel/CSV (*.xlsx *.xls *.csv)"));
    if (path.isEmpty()) {
        return;
    }

    QString error;
    const QList<CableImportRow> rows = CableImporter::readFile(path, &error);
    if (!error.isEmpty()) {
        showError(error);
        return;
    }

    CableImportSummary summary;
    if (!m_db.importCables(rows, &summary)) {
        showError(m_db.lastError());
        return;
    }

    QMessageBox::information(this,
                             QStringLiteral("导入完成"),
                             QStringLiteral("新增 %1 条，更新 %2 条，跳过 %3 条。")
                                 .arg(summary.inserted)
                                 .arg(summary.updated)
                                 .arg(summary.skipped));
}

void MainWindow::handleCableSearchReturn()
{
    const QString keyword = m_cableKeyword->text().trimmed();
    CableQrScanData scanData;
    QString decodeError;
    const bool scanDecoded = CableLabelCodec::decodePayload(keyword, &scanData, &decodeError);

    if (!scanDecoded && !keyword.isEmpty()) {
        showError(decodeError);
        return;
    }

    const QString code = scanDecoded ? scanData.code : keyword;
    if (scanDecoded && m_cableKeyword) {
        m_cableKeyword->setText(code);
    }
    if (m_addCableSearchToCache->isChecked() && !code.isEmpty()) {
        const CableRecord record = m_db.cableByCode(code);
        if (record.id > 0) {
            addCableToCache(record);
        } else {
            showError(QStringLiteral("未找到编号为“%1”的电缆。").arg(code));
        }
    }

    refreshCables();

    if (m_clearCableSearchAfterEnter->isChecked()) {
        m_cableKeyword->clear();
    }
}

void MainWindow::printSelectedCableLabels()
{
    const QList<CableRecord> records = selectedCableRecords(m_cableTable);
    QString error;
    if (!CableLabelPrinter::printLabels(records, this, &error)) {
        if (!error.isEmpty()) {
            showError(error);
        }
    }
}

void MainWindow::addSelectedCableToCache()
{
    if (!m_cableTable || !m_cableTable->model() || !m_cableTable->selectionModel()) {
        return;
    }

    const QModelIndexList rows = m_cableTable->selectionModel()->selectedRows();
    if (rows.isEmpty()) {
        showError(QStringLiteral("请先选择要加入缓存栏的电缆。"));
        return;
    }

    for (const QModelIndex &rowIndex : rows) {
        const int id = m_cableTable->model()->index(rowIndex.row(), 0).data().toInt();
        if (id > 0) {
            addCableToCache(m_db.cable(id));
        }
    }
}

QList<CableRecord> MainWindow::selectedCableRecords(QTableView *table) const
{
    QList<CableRecord> records;
    if (!table || !table->model() || !table->selectionModel()) {
        return records;
    }

    const QModelIndexList rows = table->selectionModel()->selectedRows();
    for (const QModelIndex &rowIndex : rows) {
        const QModelIndex modelIndex = table->model()->index(rowIndex.row(), 0);
        const int id = modelIndex.data().toInt();
        if (id > 0) {
            records.append(m_db.cable(id));
        }
    }
    return records;
}

void MainWindow::removeSelectedCableFromCache()
{
    const QList<QListWidgetItem *> items = m_cableCacheList->selectedItems();
    for (QListWidgetItem *item : items) {
        m_cachedCableIds.remove(item->data(Qt::UserRole).toInt());
        delete item;
    }
    updateCableCacheTitle();
}

void MainWindow::clearCableCache()
{
    m_cachedCableIds.clear();
    m_cableCacheList->clear();
    updateCableCacheTitle();
}

void MainWindow::borrowCachedCables()
{
    CableBorrowRecord record;
    record.borrower = m_cableBorrowerEdit->text();
    record.department = m_cableDepartmentEdit->text();
    record.borrowDate = m_cableBorrowDateEdit->date();
    record.expectedReturnDate = m_cableExpectedReturnEdit->date();
    record.remark = m_cableRemarkEdit->toPlainText();

    if (!m_db.borrowCables(cachedCableIds(), record)) {
        showError(m_db.lastError());
        return;
    }

    clearCableCache();
    m_cableBorrowerEdit->clear();
    m_cableDepartmentEdit->clear();
    m_cableBorrowDateEdit->setDate(QDate::currentDate());
    m_cableExpectedReturnEdit->setDate(QDate::currentDate().addDays(7));
    m_cableRemarkEdit->clear();
}

void MainWindow::returnCachedCables()
{
    if (QMessageBox::question(this,
                              QStringLiteral("确认归还"),
                              QStringLiteral("将缓存栏中的电缆登记为已归还？")) != QMessageBox::Yes) {
        return;
    }

    if (!m_db.returnCables(cachedCableIds(), m_cableActualReturnEdit->date(), m_cableRemarkEdit->toPlainText())) {
        showError(m_db.lastError());
        return;
    }

    clearCableCache();
    m_cableActualReturnEdit->setDate(QDate::currentDate());
    m_cableRemarkEdit->clear();
}

void MainWindow::showError(const QString &message)
{
    QMessageBox::warning(this, QStringLiteral("提示"), message);
}

int MainWindow::selectedId(QTableView *table) const
{
    if (!table || !table->model() || table->selectionModel()->selectedRows().isEmpty()) {
        return -1;
    }
    const QModelIndex index = table->selectionModel()->selectedRows().first();
    return table->model()->index(index.row(), 0).data().toInt();
}

void MainWindow::replaceModel(QPointer<QSqlQueryModel> &target, QSqlQueryModel *model, QTableView *table)
{
    if (target) {
        target->deleteLater();
    }
    target = model;
    table->setModel(model);
    table->resizeColumnsToContents();
}

void MainWindow::populateEquipmentCombo(QComboBox *combo, bool onlyAvailable)
{
    if (!combo) {
        return;
    }

    const QVariant previous = combo->currentData();
    combo->blockSignals(true);
    combo->clear();
    combo->addItem(QStringLiteral("请选择"), -1);
    const auto choices = m_db.equipmentChoices(onlyAvailable);
    for (const auto &choice : choices) {
        combo->addItem(choice.second, choice.first);
    }
    const int previousIndex = combo->findData(previous);
    if (previousIndex >= 0) {
        combo->setCurrentIndex(previousIndex);
    }
    combo->blockSignals(false);
}

void MainWindow::populateFilters()
{
    const QString currentCategory = m_equipmentCategoryFilter ? m_equipmentCategoryFilter->currentText() : QString();
    if (m_equipmentCategoryFilter) {
        m_equipmentCategoryFilter->blockSignals(true);
        m_equipmentCategoryFilter->clear();
        m_equipmentCategoryFilter->addItem(QStringLiteral("全部"));
        m_equipmentCategoryFilter->addItems(m_db.categories());
        const int index = m_equipmentCategoryFilter->findText(currentCategory);
        m_equipmentCategoryFilter->setCurrentIndex(index >= 0 ? index : 0);
        m_equipmentCategoryFilter->blockSignals(false);
    }

    const QString currentStatus = m_equipmentStatusFilter ? m_equipmentStatusFilter->currentText() : QString();
    if (m_equipmentStatusFilter) {
        m_equipmentStatusFilter->blockSignals(true);
        m_equipmentStatusFilter->clear();
        m_equipmentStatusFilter->addItem(QStringLiteral("全部"));
        m_equipmentStatusFilter->addItems(m_db.equipmentStatuses());
        const int index = m_equipmentStatusFilter->findText(currentStatus);
        m_equipmentStatusFilter->setCurrentIndex(index >= 0 ? index : 0);
        m_equipmentStatusFilter->blockSignals(false);
    }
}

EquipmentRecord MainWindow::equipmentFormRecord() const
{
    EquipmentRecord record;
    record.id = m_currentEquipmentId;
    record.code = m_codeEdit->text();
    record.name = m_nameEdit->text();
    record.category = m_categoryEdit->text();
    record.model = m_modelEdit->text();
    record.location = m_locationEdit->text();
    record.owner = m_ownerEdit->text();
    record.status = m_statusEdit->currentText();
    record.purchaseDate = m_purchaseDateEdit->date();
    record.remark = m_remarkEdit->toPlainText();
    return record;
}

void MainWindow::loadEquipmentToForm(int id)
{
    const EquipmentRecord record = m_db.equipment(id);
    if (record.id <= 0) {
        return;
    }

    m_currentEquipmentId = record.id;
    m_codeEdit->setText(record.code);
    m_nameEdit->setText(record.name);
    m_categoryEdit->setText(record.category);
    m_modelEdit->setText(record.model);
    m_locationEdit->setText(record.location);
    m_ownerEdit->setText(record.owner);
    m_statusEdit->setCurrentText(record.status);
    m_purchaseDateEdit->setDate(record.purchaseDate.isValid() ? record.purchaseDate : QDate::currentDate());
    m_remarkEdit->setPlainText(record.remark);
    m_saveEquipmentButton->setText(QStringLiteral("更新"));
}

QLabel *MainWindow::createMetricLabel(const QString &title)
{
    auto *label = new QLabel;
    label->setAlignment(Qt::AlignCenter);
    label->setMinimumHeight(82);
    label->setStyleSheet(QStringLiteral(
        "QLabel { background: white; border: 1px solid #d7dde5; border-radius: 6px; color: #243447; }"));
    updateMetric(label, title, 0);
    return label;
}

void MainWindow::updateMetric(QLabel *label, const QString &title, int value)
{
    label->setText(QStringLiteral("<div style='font-size:13px;color:#66788a;'>%1</div>"
                                  "<div style='font-size:28px;font-weight:700;color:#1f4e79;'>%2</div>")
                       .arg(title)
                       .arg(value));
}

void MainWindow::rebuildBars(QFormLayout *layout, const QList<QPair<QString, int>> &rows, int total)
{
    while (layout->rowCount() > 0) {
        layout->removeRow(0);
    }

    if (rows.isEmpty()) {
        layout->addRow(new QLabel(QStringLiteral("暂无数据")));
        return;
    }

    for (const auto &row : rows) {
        auto *bar = new QProgressBar;
        bar->setRange(0, qMax(1, total));
        bar->setValue(row.second);
        bar->setFormat(QStringLiteral("%1 / %2").arg(row.second).arg(total));
        layout->addRow(QStringLiteral("%1").arg(row.first), bar);
    }
}

void MainWindow::addCableToCache(const CableRecord &record)
{
    if (record.id <= 0) {
        return;
    }
    if (m_cachedCableIds.contains(record.id)) {
        statusBar()->showMessage(QStringLiteral("电缆 %1 已在缓存栏中。").arg(record.code), 3000);
        return;
    }

    auto *item = new QListWidgetItem(QStringLiteral("%1    %2 -> %3    [%4]")
                                         .arg(record.code, record.startPoint, record.endPoint, record.status),
                                     m_cableCacheList);
    item->setData(Qt::UserRole, record.id);
    m_cachedCableIds.insert(record.id);
    updateCableCacheTitle();
}

QList<int> MainWindow::cachedCableIds() const
{
    QList<int> ids;
    for (int i = 0; i < m_cableCacheList->count(); ++i) {
        ids.append(m_cableCacheList->item(i)->data(Qt::UserRole).toInt());
    }
    return ids;
}

void MainWindow::updateCableCacheTitle()
{
    if (m_cableCacheBox) {
        m_cableCacheBox->setTitle(QStringLiteral("缓存栏（%1 根）").arg(m_cableCacheList->count()));
    }
}
