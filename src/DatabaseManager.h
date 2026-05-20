#pragma once

#include <QDate>
#include <QList>
#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>

struct EquipmentRecord
{
    int id = -1;
    QString code;
    QString name;
    QString category;
    QString model;
    QString location;
    QString owner;
    QString status;
    QDate purchaseDate;
    QString remark;
};

struct BorrowRecord
{
    int id = -1;
    int equipmentId = -1;
    QString borrower;
    QString department;
    QDate borrowDate;
    QDate expectedReturnDate;
    QDate actualReturnDate;
    QString status;
    QString remark;
};

struct RepairRecord
{
    int id = -1;
    int equipmentId = -1;
    QString reporter;
    QDate reportDate;
    QString issue;
    QString status;
    QString handler;
    QDate finishedDate;
    QString solution;
};

class QSqlQueryModel;

class DatabaseManager : public QObject
{
    Q_OBJECT

public:
    explicit DatabaseManager(QObject *parent = nullptr);
    ~DatabaseManager() override;

    bool open();
    QString lastError() const;

    QStringList equipmentStatuses() const;
    QStringList repairStatuses() const;
    QStringList borrowStatuses() const;
    QStringList categories() const;

    bool saveEquipment(const EquipmentRecord &record);
    bool removeEquipment(int id);
    EquipmentRecord equipment(int id) const;
    QString equipmentDisplayName(int id) const;
    QList<QPair<int, QString>> equipmentChoices(bool onlyAvailable = false) const;

    QSqlQueryModel *createEquipmentModel(const QString &keyword,
                                         const QString &category,
                                         const QString &status,
                                         QObject *parent) const;

    bool createBorrow(const BorrowRecord &record);
    bool returnBorrow(int borrowId, const QDate &actualDate, const QString &remark);
    QSqlQueryModel *createBorrowModel(const QString &keyword, const QString &status, QObject *parent) const;

    bool createRepair(const RepairRecord &record);
    bool updateRepairStatus(int repairId,
                            const QString &status,
                            const QString &handler,
                            const QDate &finishedDate,
                            const QString &solution);
    QSqlQueryModel *createRepairModel(const QString &keyword, const QString &status, QObject *parent) const;

    QVariantMap summary() const;
    QList<QPair<QString, int>> countByStatus() const;
    QList<QPair<QString, int>> countByCategory() const;
    QSqlQueryModel *createOverdueBorrowModel(QObject *parent) const;
    QList<QVariantMap> overdueBorrows() const;

signals:
    void dataChanged();

private:
    bool ensureSchema();
    bool seedIfEmpty();
    bool execSql(const QString &sql);
    void setLastError(const QString &message) const;
    QString dbPath() const;
    QString currentEquipmentStatus(int equipmentId) const;
    bool updateEquipmentStatus(int equipmentId, const QString &status);

    QSqlDatabase m_db;
    mutable QString m_lastError;
};
