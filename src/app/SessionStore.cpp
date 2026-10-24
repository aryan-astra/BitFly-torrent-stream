#include "SessionStore.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace bitfly {
SessionStore::SessionStore(const QString& path) {
    m_db = QSqlDatabase::addDatabase("QSQLITE", QUuid::createUuid().toString());
    m_db.setDatabaseName(path);
    if (!m_db.open()) {
        m_error = m_db.lastError().text();
        return;
    }
    QSqlQuery q(m_db);
    if (!q.exec("PRAGMA user_version") || !q.next()) {
        m_error = q.lastError().text();
        return;
    }
    if (q.value(0).toInt() > 1) {
        m_error = "Database was created by a newer BitFly version.";
        return;
    }
    if (!q.exec("PRAGMA journal_mode=WAL") ||
        !q.exec("CREATE TABLE IF NOT EXISTS settings (key TEXT PRIMARY KEY,value TEXT NOT NULL)")) {
        m_error = q.lastError().text();
        return;
    }
    if (!q.exec("PRAGMA user_version=1"))
        m_error = q.lastError().text();
}
SessionStore::~SessionStore() {
    const auto name = m_db.connectionName();
    m_db.close();
    m_db = {};
    QSqlDatabase::removeDatabase(name);
}
QVariant SessionStore::get(const QString& key, const QVariant& fallback) const {
    if (!m_error.isEmpty())
        return fallback;
    QSqlQuery q(m_db);
    q.prepare("SELECT value FROM settings WHERE key=?");
    q.addBindValue(key);
    if (!q.exec()) {
        m_error = q.lastError().text();
        return fallback;
    }
    if (!q.next())
        return fallback;
    const auto doc = QJsonDocument::fromJson(q.value(0).toByteArray());
    if (!doc.isObject() || !doc.object().contains("value")) {
        m_error =
            QStringLiteral("Stored setting '%1' is invalid; refusing to overwrite saved data.")
                .arg(key);
        return fallback;
    }
    return doc.object().value("value").toVariant();
}
bool SessionStore::put(const QString& key, const QVariant& value) {
    return putMany({{key, value}});
}
bool SessionStore::putMany(const QList<QPair<QString, QVariant>>& values) {
    if (!m_error.isEmpty())
        return false;
    if (!m_db.transaction()) {
        m_error = m_db.lastError().text();
        return false;
    }
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO settings(key,value) VALUES(?,?) ON CONFLICT(key) DO UPDATE SET "
              "value=excluded.value");
    for (const auto& [key, value] : values) {
        q.bindValue(0, key);
        q.bindValue(1, QJsonDocument(QJsonObject{{"value", QJsonValue::fromVariant(value)}})
                           .toJson(QJsonDocument::Compact));
        if (!q.exec()) {
            m_error = q.lastError().text();
            m_db.rollback();
            return false;
        }
    }
    if (!m_db.commit()) {
        m_error = m_db.lastError().text();
        m_db.rollback();
        return false;
    }
    return true;
}
} // namespace bitfly
