#pragma once

#include "CableScanSupport.h"

#include <QDialog>
#include <QModelIndex>
#include <QPointer>
#include <QSet>

class QDateEdit;
class QGroupBox;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPlainTextEdit;
class QPushButton;
class QSqlQueryModel;
class QTableView;

class DatabaseManager;

class CableScanDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CableScanDialog(DatabaseManager *db, CableScanMode mode, QWidget *parent = nullptr);

private slots:
    void handleScanReturn();
    void searchCables();
    void addSelectedSearchResult();
    void addSelectedSearchResultAtRow(int row);
    void removeSelectedCacheItems();
    void clearCache();
    void submitBorrow();
    void submitReturn();

private:
    void buildUi();
    void refreshSearchResults();
    void addCableToCache(const CableRecord &record);
    void confirmAndQueueCable(const CableRecord &record, const QString &originText);
    QList<int> cachedCableIds() const;
    void updateCacheTitle();
    void resetActionForm();
    void showError(const QString &message);
    void replaceModel(QPointer<QSqlQueryModel> &target, QSqlQueryModel *model, QTableView *table);

    DatabaseManager *m_db = nullptr;
    CableScanMode m_mode = CableScanMode::Borrow;

    QLineEdit *m_scanEdit = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QTableView *m_searchTable = nullptr;
    QListWidget *m_cacheList = nullptr;
    QGroupBox *m_cacheBox = nullptr;
    QPushButton *m_actionButton = nullptr;

    QLineEdit *m_borrowerEdit = nullptr;
    QLineEdit *m_departmentEdit = nullptr;
    QDateEdit *m_borrowDateEdit = nullptr;
    QDateEdit *m_expectedReturnEdit = nullptr;
    QDateEdit *m_actualReturnEdit = nullptr;
    QPlainTextEdit *m_remarkEdit = nullptr;

    QPointer<QSqlQueryModel> m_searchModel;
    QSet<int> m_cachedIds;
};
