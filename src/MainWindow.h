#pragma once

#include "DatabaseManager.h"

#include <QList>
#include <QMainWindow>
#include <QPointer>
#include <QSet>

class QCheckBox;
class QComboBox;
class QDateEdit;
class QFormLayout;
class QGroupBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QTableView;
class QTabWidget;
class QTextEdit;
class QSqlQueryModel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void refreshAll();
    void refreshEquipment();
    void refreshBorrow();
    void refreshRepair();
    void refreshCables();
    void refreshCableBorrows();
    void refreshStats();

    void newEquipment();
    void editSelectedEquipment();
    void deleteSelectedEquipment();
    void saveEquipmentFromForm();
    void clearEquipmentForm();

    void createBorrow();
    void returnSelectedBorrow();

    void createRepair();
    void updateSelectedRepair();

    void importCables();
    void handleCableSearchReturn();
    void addSelectedCableToCache();
    void removeSelectedCableFromCache();
    void clearCableCache();
    void borrowCachedCables();
    void returnCachedCables();

private:
    void buildUi();
    QWidget *buildEquipmentTab();
    QWidget *buildBorrowTab();
    QWidget *buildRepairTab();
    QWidget *buildCableTab();
    QWidget *buildStatsTab();

    void setupTable(QTableView *table);
    void showError(const QString &message);
    int selectedId(QTableView *table) const;
    void replaceModel(QPointer<QSqlQueryModel> &target, QSqlQueryModel *model, QTableView *table);
    void populateEquipmentCombo(QComboBox *combo, bool onlyAvailable);
    void populateFilters();
    EquipmentRecord equipmentFormRecord() const;
    void loadEquipmentToForm(int id);
    QLabel *createMetricLabel(const QString &title);
    void updateMetric(QLabel *label, const QString &title, int value);
    void rebuildBars(QFormLayout *layout, const QList<QPair<QString, int>> &rows, int total);
    void addCableToCache(const CableRecord &record);
    QList<int> cachedCableIds() const;
    void updateCableCacheTitle();

    DatabaseManager m_db;
    QTabWidget *m_tabs = nullptr;

    QTableView *m_equipmentTable = nullptr;
    QLineEdit *m_equipmentKeyword = nullptr;
    QComboBox *m_equipmentCategoryFilter = nullptr;
    QComboBox *m_equipmentStatusFilter = nullptr;
    QLineEdit *m_codeEdit = nullptr;
    QLineEdit *m_nameEdit = nullptr;
    QLineEdit *m_categoryEdit = nullptr;
    QLineEdit *m_modelEdit = nullptr;
    QLineEdit *m_locationEdit = nullptr;
    QLineEdit *m_ownerEdit = nullptr;
    QComboBox *m_statusEdit = nullptr;
    QDateEdit *m_purchaseDateEdit = nullptr;
    QPlainTextEdit *m_remarkEdit = nullptr;
    QPushButton *m_saveEquipmentButton = nullptr;
    int m_currentEquipmentId = -1;
    QPointer<QSqlQueryModel> m_equipmentModel;

    QTableView *m_borrowTable = nullptr;
    QLineEdit *m_borrowKeyword = nullptr;
    QComboBox *m_borrowStatusFilter = nullptr;
    QComboBox *m_borrowEquipmentCombo = nullptr;
    QLineEdit *m_borrowerEdit = nullptr;
    QLineEdit *m_departmentEdit = nullptr;
    QDateEdit *m_borrowDateEdit = nullptr;
    QDateEdit *m_expectedReturnEdit = nullptr;
    QPlainTextEdit *m_borrowRemarkEdit = nullptr;
    QPointer<QSqlQueryModel> m_borrowModel;

    QTableView *m_repairTable = nullptr;
    QLineEdit *m_repairKeyword = nullptr;
    QComboBox *m_repairStatusFilter = nullptr;
    QComboBox *m_repairEquipmentCombo = nullptr;
    QLineEdit *m_reporterEdit = nullptr;
    QDateEdit *m_reportDateEdit = nullptr;
    QPlainTextEdit *m_issueEdit = nullptr;
    QLineEdit *m_handlerEdit = nullptr;
    QComboBox *m_repairStatusEdit = nullptr;
    QDateEdit *m_finishedDateEdit = nullptr;
    QTextEdit *m_solutionEdit = nullptr;
    QPointer<QSqlQueryModel> m_repairModel;

    QTableView *m_cableTable = nullptr;
    QLineEdit *m_cableKeyword = nullptr;
    QComboBox *m_cableStatusFilter = nullptr;
    QCheckBox *m_clearCableSearchAfterEnter = nullptr;
    QCheckBox *m_addCableSearchToCache = nullptr;
    QTableView *m_cableBorrowTable = nullptr;
    QLineEdit *m_cableBorrowKeyword = nullptr;
    QComboBox *m_cableBorrowStatusFilter = nullptr;
    QListWidget *m_cableCacheList = nullptr;
    QGroupBox *m_cableCacheBox = nullptr;
    QLineEdit *m_cableBorrowerEdit = nullptr;
    QLineEdit *m_cableDepartmentEdit = nullptr;
    QDateEdit *m_cableBorrowDateEdit = nullptr;
    QDateEdit *m_cableExpectedReturnEdit = nullptr;
    QDateEdit *m_cableActualReturnEdit = nullptr;
    QPlainTextEdit *m_cableRemarkEdit = nullptr;
    QSet<int> m_cachedCableIds;
    QPointer<QSqlQueryModel> m_cableModel;
    QPointer<QSqlQueryModel> m_cableBorrowModel;

    QLabel *m_totalMetric = nullptr;
    QLabel *m_availableMetric = nullptr;
    QLabel *m_borrowedMetric = nullptr;
    QLabel *m_repairingMetric = nullptr;
    QLabel *m_openRepairMetric = nullptr;
    QLabel *m_overdueMetric = nullptr;
    QFormLayout *m_statusBarsLayout = nullptr;
    QFormLayout *m_categoryBarsLayout = nullptr;
    QTableView *m_overdueTable = nullptr;
    QPointer<QSqlQueryModel> m_overdueModel;
};
