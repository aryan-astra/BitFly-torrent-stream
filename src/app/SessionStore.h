#pragma once
#include <QSqlDatabase>
#include <QVariant>

namespace bitfly {
class SessionStore {
  public:
    explicit SessionStore(const QString& path);
    ~SessionStore();
    QVariant get(const QString& key, const QVariant& fallback = {}) const;
    bool put(const QString& key, const QVariant& value);
    bool putMany(const QList<QPair<QString, QVariant>>& values);
    QString error() const {
        return m_error;
    }

  private:
    QSqlDatabase m_db;
    mutable QString m_error;
};
} // namespace bitfly
