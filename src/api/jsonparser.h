#pragma once

#include <QVariantMap>
#include <QVariantList>
#include <QJsonObject>
#include "apitypes.h"

namespace JsonParser {

ScUser parseUser(const QJsonObject &o);
ScTrack parseTrack(const QJsonObject &o);
ScPlaylist parsePlaylist(const QJsonObject &o);
ScStreamUrls parseStreams(const QJsonObject &o);
QVariantList parseCollection(const QJsonObject &root);
QVariantMap trackMap(const QJsonObject &o);
QVariantMap userMap(const QJsonObject &o);
QVariantMap playlistMap(const QJsonObject &o);

} // namespace JsonParser
