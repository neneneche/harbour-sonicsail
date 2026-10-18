#pragma once

#include <QObject>
#include <QCryptographicHash>
#include <QByteArray>

namespace Pkce {

QString generateVerifier();
QString challengeS256(const QString &verifier);

} // namespace Pkce
