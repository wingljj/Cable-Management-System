#include "DatabaseManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlQueryModel>

namespace {

QString dateToDb(const QDate &date)
{
    return date.isValid() ? date.toString(Qt::ISODate) : QString();
}

QDate dateFromDb(const QVariant &value)
{
    return QDate::fromString(value.toString(), Qt::ISODate);
}

void bindDate(QSqlQuery &query, const QString &name, const QDate &date)
{
    if (date.isValid()) {
        query.bindValue(name, dateToDb(date));
    } else {
        query.bindValue(name, QVariant(QVariant::String));
    }
}

} // namespace

DatabaseManager::DatabaseManager(QObject *parent)
    : QObject(parent)
{
}

DatabaseManager::~DatabaseManager()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
}

bool DatabaseManager::open()
{
    const QString connectionName = m_databasePath.isEmpty()
                                       ? QStringLiteral("dapm_connection")
                                       : QStringLiteral("dapm_connection_%1").arg(QString::number(reinterpret_cast<quintptr>(this)));
    if (QSqlDatabase::contains(connectionName)) {
        m_db = QSqlDatabase::database(connectionName);
    } else {
        m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    }

    const QDir appDir(QCoreApplication::applicationDirPath());
    appDir.mkpath(QStringLiteral("data"));
    m_db.setDatabaseName(dbPath());

    if (!m_db.open()) {
        setLastError(QStringLiteral("数据库打开失败：%1").arg(m_db.lastError().text()));
        return false;
    }

    if (!ensureSchema()) {
        return false;
    }

    return seedIfEmpty();
}

QString DatabaseManager::lastError() const
{
    return m_lastError;
}

void DatabaseManager::setDatabasePath(const QString &path)
{
    m_databasePath = path;
}

QStringList DatabaseManager::equipmentStatuses() const
{
    return {QStringLiteral("在库"), QStringLiteral("借出"), QStringLiteral("维修中"), QStringLiteral("停用"), QStringLiteral("报废")};
}

QStringList DatabaseManager::repairStatuses() const
{
    return {QStringLiteral("待处理"), QStringLiteral("处理中"), QStringLiteral("已完成"), QStringLiteral("无法修复")};
}

QStringList DatabaseManager::borrowStatuses() const
{
    return {QStringLiteral("借出中"), QStringLiteral("已归还")};
}

QStringList DatabaseManager::categories() const
{
    QStringList result;
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("SELECT DISTINCT category FROM equipment ORDER BY category"))) {
        return result;
    }

    while (query.next()) {
        result << query.value(0).toString();
    }
    return result;
}

bool DatabaseManager::saveEquipment(const EquipmentRecord &record)
{
    QSqlQuery query(m_db);
    if (record.id > 0) {
        query.prepare(QStringLiteral(
            "UPDATE equipment SET code=:code, name=:name, category=:category, model=:model, "
            "location=:location, owner=:owner, status=:status, purchase_date=:purchase_date, remark=:remark "
            "WHERE id=:id"));
        query.bindValue(QStringLiteral(":id"), record.id);
    } else {
        query.prepare(QStringLiteral(
            "INSERT INTO equipment(code, name, category, model, location, owner, status, purchase_date, remark) "
            "VALUES(:code, :name, :category, :model, :location, :owner, :status, :purchase_date, :remark)"));
    }

    query.bindValue(QStringLiteral(":code"), record.code.trimmed());
    query.bindValue(QStringLiteral(":name"), record.name.trimmed());
    query.bindValue(QStringLiteral(":category"), record.category.trimmed());
    query.bindValue(QStringLiteral(":model"), record.model.trimmed());
    query.bindValue(QStringLiteral(":location"), record.location.trimmed());
    query.bindValue(QStringLiteral(":owner"), record.owner.trimmed());
    query.bindValue(QStringLiteral(":status"), record.status.trimmed());
    bindDate(query, QStringLiteral(":purchase_date"), record.purchaseDate);
    query.bindValue(QStringLiteral(":remark"), record.remark.trimmed());

    if (!query.exec()) {
        setLastError(QStringLiteral("保存设备失败：%1").arg(query.lastError().text()));
        return false;
    }

    emit dataChanged();
    return true;
}

bool DatabaseManager::removeEquipment(int id)
{
    QSqlQuery check(m_db);
    check.prepare(QStringLiteral("SELECT COUNT(*) FROM borrow_records WHERE equipment_id=:id AND status='借出中'"));
    check.bindValue(QStringLiteral(":id"), id);
    if (!check.exec() || !check.next()) {
        setLastError(QStringLiteral("删除前检查失败：%1").arg(check.lastError().text()));
        return false;
    }
    if (check.value(0).toInt() > 0) {
        setLastError(QStringLiteral("该设备仍在借出中，不能删除。"));
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM equipment WHERE id=:id"));
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec()) {
        setLastError(QStringLiteral("删除设备失败：%1").arg(query.lastError().text()));
        return false;
    }

    emit dataChanged();
    return true;
}

EquipmentRecord DatabaseManager::equipment(int id) const
{
    EquipmentRecord record;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT id, code, name, category, model, location, owner, status, purchase_date, remark "
        "FROM equipment WHERE id=:id"));
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec() || !query.next()) {
        return record;
    }

    record.id = query.value(0).toInt();
    record.code = query.value(1).toString();
    record.name = query.value(2).toString();
    record.category = query.value(3).toString();
    record.model = query.value(4).toString();
    record.location = query.value(5).toString();
    record.owner = query.value(6).toString();
    record.status = query.value(7).toString();
    record.purchaseDate = dateFromDb(query.value(8));
    record.remark = query.value(9).toString();
    return record;
}

QString DatabaseManager::equipmentDisplayName(int id) const
{
    const EquipmentRecord record = equipment(id);
    if (record.id <= 0) {
        return QString();
    }
    return QStringLiteral("%1 - %2").arg(record.code, record.name);
}

QList<QPair<int, QString>> DatabaseManager::equipmentChoices(bool onlyAvailable) const
{
    QList<QPair<int, QString>> result;
    QSqlQuery query(m_db);
    if (onlyAvailable) {
        query.prepare(QStringLiteral(
            "SELECT id, code, name, status FROM equipment WHERE status='在库' ORDER BY code"));
    } else {
        query.prepare(QStringLiteral("SELECT id, code, name, status FROM equipment ORDER BY code"));
    }

    if (!query.exec()) {
        return result;
    }

    while (query.next()) {
        const int id = query.value(0).toInt();
        const QString text = QStringLiteral("%1 - %2（%3）")
                                 .arg(query.value(1).toString(), query.value(2).toString(), query.value(3).toString());
        result.append(qMakePair(id, text));
    }
    return result;
}

QSqlQueryModel *DatabaseManager::createEquipmentModel(const QString &keyword,
                                                      const QString &category,
                                                      const QString &status,
                                                      QObject *parent) const
{
    auto *model = new QSqlQueryModel(parent);
    QString sql = QStringLiteral(
        "SELECT id AS 'ID', code AS '编号', name AS '名称', category AS '类别', model AS '型号', "
        "location AS '位置', owner AS '责任人', status AS '状态', purchase_date AS '购置日期', remark AS '备注' "
        "FROM equipment WHERE 1=1");

    if (!keyword.trimmed().isEmpty()) {
        sql += QStringLiteral(" AND (code LIKE :keyword OR name LIKE :keyword OR model LIKE :keyword OR owner LIKE :keyword)");
    }
    if (!category.isEmpty() && category != QStringLiteral("全部")) {
        sql += QStringLiteral(" AND category=:category");
    }
    if (!status.isEmpty() && status != QStringLiteral("全部")) {
        sql += QStringLiteral(" AND status=:status");
    }
    sql += QStringLiteral(" ORDER BY code");

    QSqlQuery query(m_db);
    query.prepare(sql);
    if (!keyword.trimmed().isEmpty()) {
        query.bindValue(QStringLiteral(":keyword"), QStringLiteral("%%1%").arg(keyword.trimmed()));
    }
    if (!category.isEmpty() && category != QStringLiteral("全部")) {
        query.bindValue(QStringLiteral(":category"), category);
    }
    if (!status.isEmpty() && status != QStringLiteral("全部")) {
        query.bindValue(QStringLiteral(":status"), status);
    }
    query.exec();
    model->setQuery(query);
    return model;
}

bool DatabaseManager::createBorrow(const BorrowRecord &record)
{
    const QString status = currentEquipmentStatus(record.equipmentId);
    if (status != QStringLiteral("在库")) {
        setLastError(QStringLiteral("只有“在库”的设备/工装可以借出。当前状态：%1").arg(status));
        return false;
    }

    if (!m_db.transaction()) {
        setLastError(QStringLiteral("开启借出事务失败：%1").arg(m_db.lastError().text()));
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO borrow_records(equipment_id, borrower, department, borrow_date, expected_return_date, status, remark) "
        "VALUES(:equipment_id, :borrower, :department, :borrow_date, :expected_return_date, '借出中', :remark)"));
    query.bindValue(QStringLiteral(":equipment_id"), record.equipmentId);
    query.bindValue(QStringLiteral(":borrower"), record.borrower.trimmed());
    query.bindValue(QStringLiteral(":department"), record.department.trimmed());
    bindDate(query, QStringLiteral(":borrow_date"), record.borrowDate);
    bindDate(query, QStringLiteral(":expected_return_date"), record.expectedReturnDate);
    query.bindValue(QStringLiteral(":remark"), record.remark.trimmed());

    if (!query.exec() || !updateEquipmentStatus(record.equipmentId, QStringLiteral("借出"))) {
        m_db.rollback();
        setLastError(QStringLiteral("登记借出失败：%1").arg(query.lastError().text()));
        return false;
    }

    if (!m_db.commit()) {
        setLastError(QStringLiteral("提交借出事务失败：%1").arg(m_db.lastError().text()));
        return false;
    }

    emit dataChanged();
    return true;
}

bool DatabaseManager::returnBorrow(int borrowId, const QDate &actualDate, const QString &remark)
{
    QSqlQuery fetch(m_db);
    fetch.prepare(QStringLiteral("SELECT equipment_id, status FROM borrow_records WHERE id=:id"));
    fetch.bindValue(QStringLiteral(":id"), borrowId);
    if (!fetch.exec() || !fetch.next()) {
        setLastError(QStringLiteral("未找到借出记录。"));
        return false;
    }

    if (fetch.value(1).toString() == QStringLiteral("已归还")) {
        setLastError(QStringLiteral("该记录已经归还。"));
        return false;
    }

    const int equipmentId = fetch.value(0).toInt();
    if (!m_db.transaction()) {
        setLastError(QStringLiteral("开启归还事务失败：%1").arg(m_db.lastError().text()));
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "UPDATE borrow_records SET actual_return_date=:actual_return_date, status='已归还', "
        "remark=CASE WHEN :remark='' THEN remark ELSE :remark END WHERE id=:id"));
    bindDate(query, QStringLiteral(":actual_return_date"), actualDate);
    query.bindValue(QStringLiteral(":remark"), remark.trimmed());
    query.bindValue(QStringLiteral(":id"), borrowId);

    if (!query.exec() || !updateEquipmentStatus(equipmentId, QStringLiteral("在库"))) {
        m_db.rollback();
        setLastError(QStringLiteral("归还失败：%1").arg(query.lastError().text()));
        return false;
    }

    if (!m_db.commit()) {
        setLastError(QStringLiteral("提交归还事务失败：%1").arg(m_db.lastError().text()));
        return false;
    }

    emit dataChanged();
    return true;
}

QSqlQueryModel *DatabaseManager::createBorrowModel(const QString &keyword, const QString &status, QObject *parent) const
{
    auto *model = new QSqlQueryModel(parent);
    QString sql = QStringLiteral(
        "SELECT b.id AS 'ID', e.code AS '设备编号', e.name AS '设备名称', b.borrower AS '借用人', "
        "b.department AS '部门', b.borrow_date AS '借出日期', b.expected_return_date AS '预计归还', "
        "b.actual_return_date AS '实际归还', b.status AS '状态', b.remark AS '备注' "
        "FROM borrow_records b JOIN equipment e ON e.id=b.equipment_id WHERE 1=1");

    if (!keyword.trimmed().isEmpty()) {
        sql += QStringLiteral(" AND (e.code LIKE :keyword OR e.name LIKE :keyword OR b.borrower LIKE :keyword OR b.department LIKE :keyword)");
    }
    if (!status.isEmpty() && status != QStringLiteral("全部")) {
        sql += QStringLiteral(" AND b.status=:status");
    }
    sql += QStringLiteral(" ORDER BY b.status ASC, b.borrow_date DESC, b.id DESC");

    QSqlQuery query(m_db);
    query.prepare(sql);
    if (!keyword.trimmed().isEmpty()) {
        query.bindValue(QStringLiteral(":keyword"), QStringLiteral("%%1%").arg(keyword.trimmed()));
    }
    if (!status.isEmpty() && status != QStringLiteral("全部")) {
        query.bindValue(QStringLiteral(":status"), status);
    }
    query.exec();
    model->setQuery(query);
    return model;
}

bool DatabaseManager::createRepair(const RepairRecord &record)
{
    if (!m_db.transaction()) {
        setLastError(QStringLiteral("开启报修事务失败：%1").arg(m_db.lastError().text()));
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO repair_records(equipment_id, reporter, report_date, issue, status, handler, solution) "
        "VALUES(:equipment_id, :reporter, :report_date, :issue, '待处理', :handler, :solution)"));
    query.bindValue(QStringLiteral(":equipment_id"), record.equipmentId);
    query.bindValue(QStringLiteral(":reporter"), record.reporter.trimmed());
    bindDate(query, QStringLiteral(":report_date"), record.reportDate);
    query.bindValue(QStringLiteral(":issue"), record.issue.trimmed());
    query.bindValue(QStringLiteral(":handler"), record.handler.trimmed());
    query.bindValue(QStringLiteral(":solution"), record.solution.trimmed());

    if (!query.exec() || !updateEquipmentStatus(record.equipmentId, QStringLiteral("维修中"))) {
        m_db.rollback();
        setLastError(QStringLiteral("登记报修失败：%1").arg(query.lastError().text()));
        return false;
    }

    if (!m_db.commit()) {
        setLastError(QStringLiteral("提交报修事务失败：%1").arg(m_db.lastError().text()));
        return false;
    }

    emit dataChanged();
    return true;
}

bool DatabaseManager::updateRepairStatus(int repairId,
                                         const QString &status,
                                         const QString &handler,
                                         const QDate &finishedDate,
                                         const QString &solution)
{
    QSqlQuery fetch(m_db);
    fetch.prepare(QStringLiteral("SELECT equipment_id FROM repair_records WHERE id=:id"));
    fetch.bindValue(QStringLiteral(":id"), repairId);
    if (!fetch.exec() || !fetch.next()) {
        setLastError(QStringLiteral("未找到报修记录。"));
        return false;
    }

    const int equipmentId = fetch.value(0).toInt();
    if (!m_db.transaction()) {
        setLastError(QStringLiteral("开启维修事务失败：%1").arg(m_db.lastError().text()));
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "UPDATE repair_records SET status=:status, handler=:handler, finished_date=:finished_date, solution=:solution "
        "WHERE id=:id"));
    query.bindValue(QStringLiteral(":status"), status);
    query.bindValue(QStringLiteral(":handler"), handler.trimmed());
    bindDate(query, QStringLiteral(":finished_date"), finishedDate);
    query.bindValue(QStringLiteral(":solution"), solution.trimmed());
    query.bindValue(QStringLiteral(":id"), repairId);

    bool ok = query.exec();
    if (ok && (status == QStringLiteral("已完成"))) {
        ok = updateEquipmentStatus(equipmentId, QStringLiteral("在库"));
    } else if (ok && (status == QStringLiteral("无法修复"))) {
        ok = updateEquipmentStatus(equipmentId, QStringLiteral("停用"));
    } else if (ok) {
        ok = updateEquipmentStatus(equipmentId, QStringLiteral("维修中"));
    }

    if (!ok) {
        m_db.rollback();
        setLastError(QStringLiteral("更新维修状态失败：%1").arg(query.lastError().text()));
        return false;
    }

    if (!m_db.commit()) {
        setLastError(QStringLiteral("提交维修事务失败：%1").arg(m_db.lastError().text()));
        return false;
    }

    emit dataChanged();
    return true;
}

QSqlQueryModel *DatabaseManager::createRepairModel(const QString &keyword, const QString &status, QObject *parent) const
{
    auto *model = new QSqlQueryModel(parent);
    QString sql = QStringLiteral(
        "SELECT r.id AS 'ID', e.code AS '设备编号', e.name AS '设备名称', r.reporter AS '报修人', "
        "r.report_date AS '报修日期', r.issue AS '故障描述', r.status AS '状态', r.handler AS '处理人', "
        "r.finished_date AS '完成日期', r.solution AS '处理结果' "
        "FROM repair_records r JOIN equipment e ON e.id=r.equipment_id WHERE 1=1");

    if (!keyword.trimmed().isEmpty()) {
        sql += QStringLiteral(" AND (e.code LIKE :keyword OR e.name LIKE :keyword OR r.reporter LIKE :keyword OR r.issue LIKE :keyword)");
    }
    if (!status.isEmpty() && status != QStringLiteral("全部")) {
        sql += QStringLiteral(" AND r.status=:status");
    }
    sql += QStringLiteral(" ORDER BY r.status ASC, r.report_date DESC, r.id DESC");

    QSqlQuery query(m_db);
    query.prepare(sql);
    if (!keyword.trimmed().isEmpty()) {
        query.bindValue(QStringLiteral(":keyword"), QStringLiteral("%%1%").arg(keyword.trimmed()));
    }
    if (!status.isEmpty() && status != QStringLiteral("全部")) {
        query.bindValue(QStringLiteral(":status"), status);
    }
    query.exec();
    model->setQuery(query);
    return model;
}

QVariantMap DatabaseManager::summary() const
{
    QVariantMap result;

    auto scalar = [this](const QString &sql) -> int {
        QSqlQuery query(m_db);
        if (query.exec(sql) && query.next()) {
            return query.value(0).toInt();
        }
        return 0;
    };

    result.insert(QStringLiteral("total"), scalar(QStringLiteral("SELECT COUNT(*) FROM equipment")));
    result.insert(QStringLiteral("available"), scalar(QStringLiteral("SELECT COUNT(*) FROM equipment WHERE status='在库'")));
    result.insert(QStringLiteral("borrowed"), scalar(QStringLiteral("SELECT COUNT(*) FROM equipment WHERE status='借出'")));
    result.insert(QStringLiteral("repairing"), scalar(QStringLiteral("SELECT COUNT(*) FROM equipment WHERE status='维修中'")));
    result.insert(QStringLiteral("openRepairs"), scalar(QStringLiteral("SELECT COUNT(*) FROM repair_records WHERE status IN ('待处理','处理中')")));
    result.insert(QStringLiteral("overdue"), scalar(QStringLiteral(
                      "SELECT COUNT(*) FROM borrow_records WHERE status='借出中' "
                      "AND expected_return_date IS NOT NULL AND expected_return_date < date('now','localtime')")));
    result.insert(QStringLiteral("cableOverdue"), overdueCableBorrowCount());
    return result;
}

QList<QPair<QString, int>> DatabaseManager::countByStatus() const
{
    QList<QPair<QString, int>> result;
    QSqlQuery query(m_db);
    if (query.exec(QStringLiteral("SELECT status, COUNT(*) FROM equipment GROUP BY status ORDER BY COUNT(*) DESC"))) {
        while (query.next()) {
            result.append(qMakePair(query.value(0).toString(), query.value(1).toInt()));
        }
    }
    return result;
}

QList<QPair<QString, int>> DatabaseManager::countByCategory() const
{
    QList<QPair<QString, int>> result;
    QSqlQuery query(m_db);
    if (query.exec(QStringLiteral("SELECT category, COUNT(*) FROM equipment GROUP BY category ORDER BY COUNT(*) DESC"))) {
        while (query.next()) {
            result.append(qMakePair(query.value(0).toString(), query.value(1).toInt()));
        }
    }
    return result;
}

QSqlQueryModel *DatabaseManager::createOverdueBorrowModel(QObject *parent) const
{
    auto *model = new QSqlQueryModel(parent);
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT e.code AS '设备编号', e.name AS '设备名称', b.borrower AS '借用人', "
        "b.department AS '部门', b.expected_return_date AS '预计归还' "
        "FROM borrow_records b JOIN equipment e ON e.id=b.equipment_id "
        "WHERE b.status='借出中' AND b.expected_return_date IS NOT NULL "
        "AND b.expected_return_date < date('now','localtime') ORDER BY b.expected_return_date ASC"));
    query.exec();
    model->setQuery(query);
    return model;
}

QList<QVariantMap> DatabaseManager::overdueBorrows() const
{
    QList<QVariantMap> result;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT e.code, e.name, b.borrower, b.department, b.expected_return_date "
        "FROM borrow_records b JOIN equipment e ON e.id=b.equipment_id "
        "WHERE b.status='借出中' AND b.expected_return_date IS NOT NULL "
        "AND b.expected_return_date < date('now','localtime') "
        "ORDER BY b.expected_return_date ASC"));

    if (!query.exec()) {
        return result;
    }

    while (query.next()) {
        QVariantMap row;
        row.insert(QStringLiteral("code"), query.value(0));
        row.insert(QStringLiteral("name"), query.value(1));
        row.insert(QStringLiteral("borrower"), query.value(2));
        row.insert(QStringLiteral("department"), query.value(3));
        row.insert(QStringLiteral("date"), query.value(4));
        result.append(row);
    }
    return result;
}

QStringList DatabaseManager::cableStatuses() const
{
    return {QStringLiteral("在库"), QStringLiteral("借出"), QStringLiteral("逾期未还")};
}

QStringList DatabaseManager::cableBorrowStatuses() const
{
    return {QStringLiteral("借出中"), QStringLiteral("已归还")};
}

bool DatabaseManager::importCables(const QList<CableImportRow> &rows, CableImportSummary *summary)
{
    CableImportSummary localSummary;
    QSet<QString> existingCodes;
    QSqlQuery existingQuery(m_db);
    if (!existingQuery.exec(QStringLiteral("SELECT code FROM cables"))) {
        setLastError(QStringLiteral("读取已有电缆编号失败：%1").arg(existingQuery.lastError().text()));
        return false;
    }
    while (existingQuery.next()) {
        existingCodes.insert(existingQuery.value(0).toString());
    }

    if (!m_db.transaction()) {
        setLastError(QStringLiteral("开启电缆导入事务失败：%1").arg(m_db.lastError().text()));
        return false;
    }

    QSqlQuery insertQuery(m_db);
    insertQuery.prepare(QStringLiteral(
        "INSERT INTO cables(code, start_point, end_point, usage_expiry_date, remark, status) "
        "VALUES(:code, :start_point, :end_point, :usage_expiry_date, :remark, '在库')"));

    QSqlQuery updateQuery(m_db);
    updateQuery.prepare(QStringLiteral(
        "UPDATE cables SET start_point=:start_point, end_point=:end_point, "
        "usage_expiry_date=CASE WHEN :usage_expiry_specified=1 THEN :usage_expiry_date ELSE usage_expiry_date END, "
        "remark=CASE WHEN :remark_specified=1 THEN :remark ELSE remark END WHERE code=:code"));

    for (const CableImportRow &row : rows) {
        const QString code = row.code.trimmed();
        if (code.isEmpty()) {
            ++localSummary.skipped;
            continue;
        }

        QSqlQuery &query = existingCodes.contains(code) ? updateQuery : insertQuery;
        if (existingCodes.contains(code)) {
            ++localSummary.updated;
        } else {
            ++localSummary.inserted;
            existingCodes.insert(code);
        }

        query.bindValue(QStringLiteral(":code"), code);
        query.bindValue(QStringLiteral(":start_point"), row.startPoint.trimmed());
        query.bindValue(QStringLiteral(":end_point"), row.endPoint.trimmed());
        bindDate(query, QStringLiteral(":usage_expiry_date"), row.usageExpiryDate);
        query.bindValue(QStringLiteral(":remark"), row.remark.trimmed());
        query.bindValue(QStringLiteral(":usage_expiry_specified"), row.usageExpiryDateSpecified ? 1 : 0);
        query.bindValue(QStringLiteral(":remark_specified"), row.remarkSpecified ? 1 : 0);
        if (!query.exec()) {
            m_db.rollback();
            setLastError(QStringLiteral("导入电缆失败：%1").arg(query.lastError().text()));
            return false;
        }
    }

    if (!m_db.commit()) {
        setLastError(QStringLiteral("提交电缆导入事务失败：%1").arg(m_db.lastError().text()));
        return false;
    }

    if (summary) {
        *summary = localSummary;
    }
    emit dataChanged();
    return true;
}

bool DatabaseManager::saveCable(const CableRecord &record)
{
    const QString code = record.code.trimmed();
    if (code.isEmpty()) {
        setLastError(QStringLiteral("电缆编号不能为空。"));
        return false;
    }

    QSqlQuery query(m_db);
    if (record.id > 0) {
        query.prepare(QStringLiteral(
            "UPDATE cables SET code=:code, start_point=:start_point, end_point=:end_point, "
            "usage_expiry_date=:usage_expiry_date, remark=:remark WHERE id=:id"));
        query.bindValue(QStringLiteral(":id"), record.id);
    } else {
        query.prepare(QStringLiteral(
            "INSERT INTO cables(code, start_point, end_point, usage_expiry_date, remark, status) "
            "VALUES(:code, :start_point, :end_point, :usage_expiry_date, :remark, '在库')"));
    }

    query.bindValue(QStringLiteral(":code"), code);
    query.bindValue(QStringLiteral(":start_point"), record.startPoint.trimmed());
    query.bindValue(QStringLiteral(":end_point"), record.endPoint.trimmed());
    bindDate(query, QStringLiteral(":usage_expiry_date"), record.usageExpiryDate);
    query.bindValue(QStringLiteral(":remark"), record.remark.trimmed());

    if (!query.exec()) {
        setLastError(QStringLiteral("保存电缆台账失败：%1").arg(query.lastError().text()));
        return false;
    }

    emit dataChanged();
    return true;
}

bool DatabaseManager::updateCableUsageExpiry(const QList<int> &cableIds, const QDate &usageExpiryDate)
{
    if (cableIds.isEmpty()) {
        setLastError(QStringLiteral("请先选择要修改使用期限的电缆。"));
        return false;
    }

    if (!m_db.transaction()) {
        setLastError(QStringLiteral("开启使用期限批量修改事务失败：%1").arg(m_db.lastError().text()));
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("UPDATE cables SET usage_expiry_date=:usage_expiry_date WHERE id=:id"));
    for (int cableId : cableIds) {
        bindDate(query, QStringLiteral(":usage_expiry_date"), usageExpiryDate);
        query.bindValue(QStringLiteral(":id"), cableId);
        if (!query.exec()) {
            m_db.rollback();
            setLastError(QStringLiteral("批量修改使用期限失败：%1").arg(query.lastError().text()));
            return false;
        }
    }

    if (!m_db.commit()) {
        setLastError(QStringLiteral("提交使用期限批量修改失败：%1").arg(m_db.lastError().text()));
        return false;
    }

    emit dataChanged();
    return true;
}

bool DatabaseManager::removeCable(int id)
{
    QSqlQuery check(m_db);
    check.prepare(QStringLiteral("SELECT COUNT(*) FROM cable_borrow_records WHERE cable_id=:id"));
    check.bindValue(QStringLiteral(":id"), id);
    if (!check.exec() || !check.next()) {
        setLastError(QStringLiteral("删除前检查电缆借还历史失败：%1").arg(check.lastError().text()));
        return false;
    }
    if (check.value(0).toInt() > 0) {
        setLastError(QStringLiteral("该电缆已有借还历史，不能删除。"));
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM cables WHERE id=:id"));
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec()) {
        setLastError(QStringLiteral("删除电缆失败：%1").arg(query.lastError().text()));
        return false;
    }

    emit dataChanged();
    return true;
}

QSqlQueryModel *DatabaseManager::createCableModel(const QString &keyword, const QString &status, QObject *parent) const
{
    auto *model = new QSqlQueryModel(parent);
    QString sql = QStringLiteral(
        "SELECT c.id AS 'ID', c.code AS '编号', c.end_point AS '终端', c.start_point AS '始端', "
        "c.usage_expiry_date AS '使用期限', c.status AS '状态', c.remark AS '台账备注', "
        "b.borrower AS '借用人', b.department AS '部门', b.borrow_date AS '借出日期', "
        "b.expected_return_date AS '预计归还', b.actual_return_date AS '实际归还', "
        "b.status AS '借还状态', b.remark AS '借还备注' "
        "FROM cables c LEFT JOIN cable_borrow_records b ON b.id=("
        "SELECT b2.id FROM cable_borrow_records b2 WHERE b2.cable_id=c.id "
        "ORDER BY CASE WHEN b2.status='借出中' THEN 0 ELSE 1 END, b2.borrow_date DESC, b2.id DESC LIMIT 1"
        ") WHERE 1=1");

    if (!keyword.trimmed().isEmpty()) {
        sql += QStringLiteral(" AND (c.code LIKE :keyword OR c.start_point LIKE :keyword OR c.end_point LIKE :keyword "
                              "OR c.remark LIKE :keyword OR b.borrower LIKE :keyword OR b.department LIKE :keyword)");
    }
    if (status == QStringLiteral("逾期未还")) {
        sql += QStringLiteral(" AND b.status='借出中' AND b.expected_return_date IS NOT NULL "
                              "AND b.expected_return_date < date('now','localtime')");
    } else if (!status.isEmpty() && status != QStringLiteral("全部")) {
        sql += QStringLiteral(" AND c.status=:status");
    }
    sql += QStringLiteral(" ORDER BY c.code");

    QSqlQuery query(m_db);
    query.prepare(sql);
    if (!keyword.trimmed().isEmpty()) {
        query.bindValue(QStringLiteral(":keyword"), QStringLiteral("%%1%").arg(keyword.trimmed()));
    }
    if (!status.isEmpty() && status != QStringLiteral("全部") && status != QStringLiteral("逾期未还")) {
        query.bindValue(QStringLiteral(":status"), status);
    }
    query.exec();
    model->setQuery(query);
    return model;
}

QSqlQueryModel *DatabaseManager::createOverdueCableBorrowModel(QObject *parent) const
{
    auto *model = new QSqlQueryModel(parent);
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT c.code AS '编号', c.end_point AS '终端', c.start_point AS '始端', "
        "b.borrower AS '借用人', b.department AS '部门', b.borrow_date AS '借出日期', "
        "b.expected_return_date AS '预计归还', "
        "CAST(julianday(date('now','localtime')) - julianday(b.expected_return_date) AS INTEGER) AS '逾期天数', "
        "b.remark AS '备注' "
        "FROM cable_borrow_records b JOIN cables c ON c.id=b.cable_id "
        "WHERE b.status='借出中' AND b.expected_return_date IS NOT NULL "
        "AND b.expected_return_date < date('now','localtime') "
        "ORDER BY b.expected_return_date ASC, c.code ASC"));
    query.exec();
    model->setQuery(query);
    return model;
}

int DatabaseManager::overdueCableBorrowCount() const
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM cable_borrow_records "
        "WHERE status='借出中' AND expected_return_date IS NOT NULL "
        "AND expected_return_date < date('now','localtime')"));
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

QSqlQueryModel *DatabaseManager::createCableBorrowModel(const QString &keyword,
                                                        const QString &status,
                                                        QObject *parent) const
{
    auto *model = new QSqlQueryModel(parent);
    QString sql = QStringLiteral(
        "SELECT b.id AS 'ID', c.code AS '电缆编号', c.end_point AS '终端', c.start_point AS '始端', "
        "b.borrower AS '借用人', b.department AS '部门', b.borrow_date AS '借出日期', "
        "b.expected_return_date AS '预计归还', b.actual_return_date AS '实际归还', "
        "b.status AS '状态', b.remark AS '备注' "
        "FROM cable_borrow_records b JOIN cables c ON c.id=b.cable_id WHERE 1=1");

    if (!keyword.trimmed().isEmpty()) {
        sql += QStringLiteral(" AND (c.code LIKE :keyword OR c.start_point LIKE :keyword OR c.end_point LIKE :keyword "
                              "OR b.borrower LIKE :keyword OR b.department LIKE :keyword)");
    }
    if (!status.isEmpty() && status != QStringLiteral("全部")) {
        sql += QStringLiteral(" AND b.status=:status");
    }
    sql += QStringLiteral(" ORDER BY b.status ASC, b.borrow_date DESC, b.id DESC");

    QSqlQuery query(m_db);
    query.prepare(sql);
    if (!keyword.trimmed().isEmpty()) {
        query.bindValue(QStringLiteral(":keyword"), QStringLiteral("%%1%").arg(keyword.trimmed()));
    }
    if (!status.isEmpty() && status != QStringLiteral("全部")) {
        query.bindValue(QStringLiteral(":status"), status);
    }
    query.exec();
    model->setQuery(query);
    return model;
}

CableRecord DatabaseManager::cable(int id) const
{
    CableRecord record;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT id, code, start_point, end_point, usage_expiry_date, remark, status FROM cables WHERE id=:id"));
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec() || !query.next()) {
        return record;
    }

    record.id = query.value(0).toInt();
    record.code = query.value(1).toString();
    record.startPoint = query.value(2).toString();
    record.endPoint = query.value(3).toString();
    record.usageExpiryDate = dateFromDb(query.value(4));
    record.remark = query.value(5).toString();
    record.status = query.value(6).toString();
    return record;
}

CableRecord DatabaseManager::cableByCode(const QString &code) const
{
    CableRecord record;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT id, code, start_point, end_point, usage_expiry_date, remark, status FROM cables WHERE code=:code"));
    query.bindValue(QStringLiteral(":code"), code.trimmed());
    if (!query.exec() || !query.next()) {
        return record;
    }

    record.id = query.value(0).toInt();
    record.code = query.value(1).toString();
    record.startPoint = query.value(2).toString();
    record.endPoint = query.value(3).toString();
    record.usageExpiryDate = dateFromDb(query.value(4));
    record.remark = query.value(5).toString();
    record.status = query.value(6).toString();
    return record;
}

int DatabaseManager::cableIdByCode(const QString &code) const
{
    return cableByCode(code).id;
}

QString DatabaseManager::cableStatus(int cableId) const
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT status FROM cables WHERE id=:id"));
    query.bindValue(QStringLiteral(":id"), cableId);
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return QString();
}

bool DatabaseManager::borrowCables(const QList<int> &cableIds, const CableBorrowRecord &record)
{
    if (cableIds.isEmpty()) {
        setLastError(QStringLiteral("请先将电缆加入缓存栏。"));
        return false;
    }
    if (record.borrower.trimmed().isEmpty()) {
        setLastError(QStringLiteral("请填写借用人。"));
        return false;
    }

    if (!m_db.transaction()) {
        setLastError(QStringLiteral("开启电缆借出事务失败：%1").arg(m_db.lastError().text()));
        return false;
    }

    for (int cableId : cableIds) {
        const QString status = cableStatus(cableId);
        if (status != QStringLiteral("在库")) {
            m_db.rollback();
            setLastError(QStringLiteral("只有“在库”的电缆可以借出。当前状态：%1").arg(status));
            return false;
        }

        QSqlQuery query(m_db);
        query.prepare(QStringLiteral(
            "INSERT INTO cable_borrow_records(cable_id, borrower, department, borrow_date, expected_return_date, status, remark) "
            "VALUES(:cable_id, :borrower, :department, :borrow_date, :expected_return_date, '借出中', :remark)"));
        query.bindValue(QStringLiteral(":cable_id"), cableId);
        query.bindValue(QStringLiteral(":borrower"), record.borrower.trimmed());
        query.bindValue(QStringLiteral(":department"), record.department.trimmed());
        bindDate(query, QStringLiteral(":borrow_date"), record.borrowDate);
        bindDate(query, QStringLiteral(":expected_return_date"), record.expectedReturnDate);
        query.bindValue(QStringLiteral(":remark"), record.remark.trimmed());

        if (!query.exec() || !updateCableStatus(cableId, QStringLiteral("借出"))) {
            m_db.rollback();
            setLastError(QStringLiteral("登记电缆借出失败：%1").arg(query.lastError().text()));
            return false;
        }
    }

    if (!m_db.commit()) {
        setLastError(QStringLiteral("提交电缆借出事务失败：%1").arg(m_db.lastError().text()));
        return false;
    }

    emit dataChanged();
    return true;
}

bool DatabaseManager::returnCables(const QList<int> &cableIds, const QDate &actualDate, const QString &remark)
{
    if (cableIds.isEmpty()) {
        setLastError(QStringLiteral("请先将电缆加入缓存栏。"));
        return false;
    }

    if (!m_db.transaction()) {
        setLastError(QStringLiteral("开启电缆归还事务失败：%1").arg(m_db.lastError().text()));
        return false;
    }

    for (int cableId : cableIds) {
        const QString status = cableStatus(cableId);
        if (status != QStringLiteral("借出")) {
            m_db.rollback();
            setLastError(QStringLiteral("只有“借出”的电缆可以归还。当前状态：%1").arg(status));
            return false;
        }

        const int borrowId = openCableBorrowRecordId(cableId);
        if (borrowId <= 0) {
            m_db.rollback();
            setLastError(QStringLiteral("未找到电缆的未归还记录。"));
            return false;
        }

        QSqlQuery query(m_db);
        query.prepare(QStringLiteral(
            "UPDATE cable_borrow_records SET actual_return_date=:actual_return_date, status='已归还', "
            "remark=CASE WHEN :remark='' THEN remark ELSE :remark END WHERE id=:id"));
        bindDate(query, QStringLiteral(":actual_return_date"), actualDate);
        query.bindValue(QStringLiteral(":remark"), remark.trimmed());
        query.bindValue(QStringLiteral(":id"), borrowId);

        if (!query.exec() || !updateCableStatus(cableId, QStringLiteral("在库"))) {
            m_db.rollback();
            setLastError(QStringLiteral("登记电缆归还失败：%1").arg(query.lastError().text()));
            return false;
        }
    }

    if (!m_db.commit()) {
        setLastError(QStringLiteral("提交电缆归还事务失败：%1").arg(m_db.lastError().text()));
        return false;
    }

    emit dataChanged();
    return true;
}

bool DatabaseManager::ensureSchema()
{
    const QStringList statements = {
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS equipment ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "code TEXT NOT NULL UNIQUE,"
            "name TEXT NOT NULL,"
            "category TEXT NOT NULL,"
            "model TEXT,"
            "location TEXT,"
            "owner TEXT,"
            "status TEXT NOT NULL DEFAULT '在库',"
            "purchase_date TEXT,"
            "remark TEXT)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS borrow_records ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "equipment_id INTEGER NOT NULL,"
            "borrower TEXT NOT NULL,"
            "department TEXT,"
            "borrow_date TEXT NOT NULL,"
            "expected_return_date TEXT,"
            "actual_return_date TEXT,"
            "status TEXT NOT NULL DEFAULT '借出中',"
            "remark TEXT,"
            "FOREIGN KEY(equipment_id) REFERENCES equipment(id) ON DELETE CASCADE)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS repair_records ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "equipment_id INTEGER NOT NULL,"
            "reporter TEXT NOT NULL,"
            "report_date TEXT NOT NULL,"
            "issue TEXT NOT NULL,"
            "status TEXT NOT NULL DEFAULT '待处理',"
            "handler TEXT,"
            "finished_date TEXT,"
            "solution TEXT,"
            "FOREIGN KEY(equipment_id) REFERENCES equipment(id) ON DELETE CASCADE)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS cables ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "code TEXT NOT NULL UNIQUE,"
            "start_point TEXT NOT NULL,"
            "end_point TEXT NOT NULL,"
            "usage_expiry_date TEXT,"
            "remark TEXT,"
            "status TEXT NOT NULL DEFAULT '在库')"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS cable_borrow_records ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "cable_id INTEGER NOT NULL,"
            "borrower TEXT NOT NULL,"
            "department TEXT,"
            "borrow_date TEXT NOT NULL,"
            "expected_return_date TEXT,"
            "actual_return_date TEXT,"
            "status TEXT NOT NULL DEFAULT '借出中',"
            "remark TEXT,"
            "FOREIGN KEY(cable_id) REFERENCES cables(id) ON DELETE CASCADE)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS schema_migrations ("
            "name TEXT PRIMARY KEY,"
            "applied_at TEXT NOT NULL DEFAULT (datetime('now','localtime')))"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_equipment_code ON equipment(code)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_borrow_equipment ON borrow_records(equipment_id)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_repair_equipment ON repair_records(equipment_id)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_cables_code ON cables(code)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_cables_status ON cables(status)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_cable_borrow_cable ON cable_borrow_records(cable_id)")
    };

    for (const QString &sql : statements) {
        if (!execSql(sql)) {
            return false;
        }
    }

    return ensureColumn(QStringLiteral("cables"),
                        QStringLiteral("usage_expiry_date"),
                        QStringLiteral("TEXT")) &&
           ensureColumn(QStringLiteral("cables"), QStringLiteral("remark"), QStringLiteral("TEXT")) &&
           runCableEndpointSwapMigration();
}

bool DatabaseManager::seedIfEmpty()
{
    QSqlQuery count(m_db);
    if (!count.exec(QStringLiteral("SELECT COUNT(*) FROM equipment")) || !count.next()) {
        setLastError(QStringLiteral("初始化检查失败：%1").arg(count.lastError().text()));
        return false;
    }
    if (count.value(0).toInt() > 0) {
        return true;
    }

    const QList<EquipmentRecord> records = {
        {-1, QStringLiteral("SB-0001"), QStringLiteral("三坐标测量仪"), QStringLiteral("检测设备"), QStringLiteral("CMM-860"), QStringLiteral("质检室"), QStringLiteral("张工"), QStringLiteral("在库"), QDate(2024, 3, 15), QStringLiteral("精密测量")},
        {-1, QStringLiteral("GZ-0002"), QStringLiteral("定位夹具 A"), QStringLiteral("工装夹具"), QStringLiteral("JIG-A12"), QStringLiteral("一号线"), QStringLiteral("李工"), QStringLiteral("在库"), QDate(2023, 8, 2), QStringLiteral("装配定位")},
        {-1, QStringLiteral("SB-0003"), QStringLiteral("扭矩扳手"), QStringLiteral("工具"), QStringLiteral("TW-200"), QStringLiteral("工具间"), QStringLiteral("王工"), QStringLiteral("借出"), QDate(2022, 11, 8), QStringLiteral("需定期校准")},
        {-1, QStringLiteral("GZ-0004"), QStringLiteral("压装治具"), QStringLiteral("工装夹具"), QStringLiteral("PRESS-18"), QStringLiteral("二号线"), QStringLiteral("赵工"), QStringLiteral("维修中"), QDate(2021, 6, 22), QStringLiteral("液压部件检查中")},
        {-1, QStringLiteral("SB-0005"), QStringLiteral("绝缘测试仪"), QStringLiteral("检测设备"), QStringLiteral("IT-500"), QStringLiteral("电检区"), QStringLiteral("陈工"), QStringLiteral("在库"), QDate(2024, 1, 10), QStringLiteral("高压测试")}
    };

    if (!m_db.transaction()) {
        setLastError(QStringLiteral("初始化事务失败：%1").arg(m_db.lastError().text()));
        return false;
    }

    for (const EquipmentRecord &record : records) {
        if (!saveEquipment(record)) {
            m_db.rollback();
            return false;
        }
    }

    QSqlQuery borrow(m_db);
    borrow.prepare(QStringLiteral(
        "INSERT INTO borrow_records(equipment_id, borrower, department, borrow_date, expected_return_date, status, remark) "
        "VALUES((SELECT id FROM equipment WHERE code='SB-0003'), '刘强', '装配部', :borrow_date, :expected_date, '借出中', '生产线临时借用')"));
    bindDate(borrow, QStringLiteral(":borrow_date"), QDate::currentDate().addDays(-5));
    bindDate(borrow, QStringLiteral(":expected_date"), QDate::currentDate().addDays(7));
    if (!borrow.exec()) {
        m_db.rollback();
        setLastError(QStringLiteral("初始化借出记录失败：%1").arg(borrow.lastError().text()));
        return false;
    }

    QSqlQuery repair(m_db);
    repair.prepare(QStringLiteral(
        "INSERT INTO repair_records(equipment_id, reporter, report_date, issue, status, handler, solution) "
        "VALUES((SELECT id FROM equipment WHERE code='GZ-0004'), '赵工', :report_date, '压装定位销磨损，重复定位精度下降', '处理中', '维修组', '')"));
    bindDate(repair, QStringLiteral(":report_date"), QDate::currentDate().addDays(-2));
    if (!repair.exec()) {
        m_db.rollback();
        setLastError(QStringLiteral("初始化报修记录失败：%1").arg(repair.lastError().text()));
        return false;
    }

    return m_db.commit();
}

bool DatabaseManager::execSql(const QString &sql)
{
    QSqlQuery query(m_db);
    if (!query.exec(sql)) {
        setLastError(QStringLiteral("执行 SQL 失败：%1\n%2").arg(query.lastError().text(), sql));
        return false;
    }
    return true;
}

bool DatabaseManager::ensureColumn(const QString &table, const QString &column, const QString &definition)
{
    if (tableHasColumn(table, column)) {
        return true;
    }
    return execSql(QStringLiteral("ALTER TABLE %1 ADD COLUMN %2 %3").arg(table, column, definition));
}

bool DatabaseManager::tableHasColumn(const QString &table, const QString &column) const
{
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table))) {
        return false;
    }

    while (query.next()) {
        if (query.value(1).toString() == column) {
            return true;
        }
    }
    return false;
}

bool DatabaseManager::migrationApplied(const QString &name) const
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT 1 FROM schema_migrations WHERE name=:name"));
    query.bindValue(QStringLiteral(":name"), name);
    return query.exec() && query.next();
}

bool DatabaseManager::markMigrationApplied(const QString &name)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("INSERT OR IGNORE INTO schema_migrations(name) VALUES(:name)"));
    query.bindValue(QStringLiteral(":name"), name);
    if (!query.exec()) {
        setLastError(QStringLiteral("记录数据库迁移失败：%1").arg(query.lastError().text()));
        return false;
    }
    return true;
}

bool DatabaseManager::runCableEndpointSwapMigration()
{
    const QString migrationName = QStringLiteral("20260523_swap_cable_endpoints");
    if (migrationApplied(migrationName)) {
        return true;
    }

    if (!m_db.transaction()) {
        setLastError(QStringLiteral("开启电缆端点迁移事务失败：%1").arg(m_db.lastError().text()));
        return false;
    }

    QSqlQuery swap(m_db);
    if (!swap.exec(QStringLiteral("UPDATE cables SET start_point=end_point, end_point=start_point"))) {
        m_db.rollback();
        setLastError(QStringLiteral("交换电缆始端/终端失败：%1").arg(swap.lastError().text()));
        return false;
    }

    if (!markMigrationApplied(migrationName)) {
        m_db.rollback();
        return false;
    }

    if (!m_db.commit()) {
        setLastError(QStringLiteral("提交电缆端点迁移失败：%1").arg(m_db.lastError().text()));
        return false;
    }
    return true;
}

void DatabaseManager::setLastError(const QString &message) const
{
    m_lastError = message;
}

QString DatabaseManager::dbPath() const
{
    if (!m_databasePath.isEmpty()) {
        return m_databasePath;
    }
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("data/equipment.db"));
}

QString DatabaseManager::currentEquipmentStatus(int equipmentId) const
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT status FROM equipment WHERE id=:id"));
    query.bindValue(QStringLiteral(":id"), equipmentId);
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return QString();
}

bool DatabaseManager::updateEquipmentStatus(int equipmentId, const QString &status)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("UPDATE equipment SET status=:status WHERE id=:id"));
    query.bindValue(QStringLiteral(":status"), status);
    query.bindValue(QStringLiteral(":id"), equipmentId);
    if (!query.exec()) {
        setLastError(QStringLiteral("更新设备状态失败：%1").arg(query.lastError().text()));
        return false;
    }
    return true;
}

bool DatabaseManager::updateCableStatus(int cableId, const QString &status)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("UPDATE cables SET status=:status WHERE id=:id"));
    query.bindValue(QStringLiteral(":status"), status);
    query.bindValue(QStringLiteral(":id"), cableId);
    if (!query.exec()) {
        setLastError(QStringLiteral("更新电缆状态失败：%1").arg(query.lastError().text()));
        return false;
    }
    return true;
}

int DatabaseManager::openCableBorrowRecordId(int cableId) const
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT id FROM cable_borrow_records WHERE cable_id=:cable_id AND status='借出中' "
        "ORDER BY borrow_date DESC, id DESC LIMIT 1"));
    query.bindValue(QStringLiteral(":cable_id"), cableId);
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    return -1;
}
