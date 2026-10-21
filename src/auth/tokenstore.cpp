#include "tokenstore.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QDateTime>

TokenStore::TokenStore(QObject *parent)
    : QObject(parent)
{
}

QString TokenStore::filePath() const
{
    // DataLocation survives cache clearing; CacheLocation may be wiped
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/tokens.json");
}

QJsonObject TokenStore::readAll() const
{
    QFile f(filePath());
    if (!f.open(QIODevice::ReadOnly))
        return QJsonObject();
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    return doc.object();
}

void TokenStore::writeAll(const QJsonObject &obj) const
{
    QSaveFile f(filePath());
    if (!f.open(QIODevice::WriteOnly))
        return;
    f.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    f.commit();
    // best-effort 0600
    QFile::setPermissions(filePath(), QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

TokenStore::Token TokenStore::load(const QString &mode) const
{
    Token t;
    const QJsonObject root = readAll();
    const QJsonObject o = root.value(mode).toObject();
    t.accessToken = o.value(QStringLiteral("access_token")).toString();
    t.refreshToken = o.value(QStringLiteral("refresh_token")).toString();
    t.expiresAt = qint64(o.value(QStringLiteral("expires_at")).toDouble());
    t.tokenType = o.value(QStringLiteral("token_type")).toString();
    t.scope = o.value(QStringLiteral("scope")).toString();
    t.mode = mode;
    return t;
}

void TokenStore::save(const Token &token)
{
    QJsonObject root = readAll();
    QJsonObject o;
    o.insert(QStringLiteral("access_token"), token.accessToken);
    o.insert(QStringLiteral("refresh_token"), token.refreshToken);
    o.insert(QStringLiteral("expires_at"), double(token.expiresAt));
    o.insert(QStringLiteral("token_type"), token.tokenType);
    o.insert(QStringLiteral("scope"), token.scope);
    root.insert(token.mode, o);
    writeAll(root);
}

void TokenStore::clear(const QString &mode)
{
    QJsonObject root = readAll();
    root.remove(mode);
    writeAll(root);
}

void TokenStore::clearAll()
{
    writeAll(QJsonObject());
}
