#include "pkce.h"

#include <QDateTime>
#include <QtGlobal>

namespace {

QByteArray base64Url(const QByteArray &in)
{
    QByteArray b = in.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    return b;
}

} // namespace

namespace Pkce {

QString generateVerifier()
{
    static const char charset[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
    const int len = 64; // within 43..128
    static bool seeded = false;
    if (!seeded) {
        qsrand(uint(QDateTime::currentMSecsSinceEpoch()));
        seeded = true;
    }
    QByteArray out;
    out.reserve(len);
    for (int i = 0; i < len; ++i) {
        const int r = qrand() % int(sizeof(charset) - 1);
        out.append(charset[r]);
    }
    return QString::fromLatin1(out);
}

QString challengeS256(const QString &verifier)
{
    const QByteArray hash = QCryptographicHash::hash(verifier.toUtf8(), QCryptographicHash::Sha256);
    return QString::fromLatin1(base64Url(hash));
}

} // namespace Pkce
