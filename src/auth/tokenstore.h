#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>

class TokenStore : public QObject
{
    Q_OBJECT
public:
    explicit TokenStore(QObject *parent = nullptr);

    struct Token {
        QString accessToken;
        QString refreshToken;
        qint64 expiresAt = 0; // epoch ms
        QString tokenType;
        QString scope;
        QString mode; // "anon" | "user"

        bool isValid(qint64 nowMs, qint64 slackMs = 60000) const {
            return !accessToken.isEmpty() && (expiresAt - slackMs) > nowMs;
        }
    };

    Token load(const QString &mode) const;
    void save(const Token &token);
    void clear(const QString &mode);
    void clearAll();

private:
    QString filePath() const;
    QJsonObject readAll() const;
    void writeAll(const QJsonObject &obj) const;
};
