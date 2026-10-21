#include "app.h"

#include <QQmlContext>
#include <QQmlEngine>

#include "api/apiclient.h"
#include "api/clientidresolver.h"
#include "auth/authmanager.h"
#include "player/playercontroller.h"
#include "player/trackqueue.h"
#include "util/settings.h"
#include "util/coverimageprovider.h"
#include "util/downloadmanager.h"
#include "util/mpriscontroller.h"

#include <sailfishapp.h>

AppController::AppController(QObject *parent)
    : QObject(parent)
{
    m_settings = new Settings(this);
    m_auth = new AuthManager(m_settings, this);
    m_api = new ApiClient(m_auth, this);
    m_queue = new TrackQueue(this);
    m_player = new PlayerController(m_api, m_queue, this);
    DownloadManager *dl = new DownloadManager(m_api, this);
    // Auto-download once ~60% of the track has been played (like SubMusic)
    connect(m_player, &PlayerController::positionChanged, this,
            [this, dl]() {
        if (!m_settings->autoDownloadPlayed())
            return;
        const int dur = m_player->duration();
        const int pos = m_player->position();
        if (dur <= 0 || pos < int(dur * 0.6))
            return;
        dl->autoDownload(m_queue->currentTrack(), m_settings->saveToMusic());
    });

    ClientIdResolver *resolver = new ClientIdResolver(this);
    connect(resolver, &ClientIdResolver::resolved, this, [this](const QString &id) {
        m_api->setPublicClientId(id);
    });
    connect(m_api, &ApiClient::unauthorized, this, [this, resolver]() {
        resolver->reset();
        resolver->resolve();
    });
    resolver->resolve();

    m_view = SailfishApp::createView();
    m_view->engine()->addImageProvider(QStringLiteral("cover"),
                                       new CoverImageProvider(m_api));

    QQmlContext *ctx = m_view->rootContext();
    ctx->setContextProperty(QStringLiteral("api"), m_api);
    ctx->setContextProperty(QStringLiteral("auth"), m_auth);
    ctx->setContextProperty(QStringLiteral("player"), m_player);
    ctx->setContextProperty(QStringLiteral("queue"), m_queue);
    ctx->setContextProperty(QStringLiteral("settings"), m_settings);
    ctx->setContextProperty(QStringLiteral("downloads"), dl);

    // Lock screen / system media controls (MPRIS)
    MprisService *mpris = new MprisService(m_player, m_queue, this);
    mpris->registerOnBus();

    m_view->setSource(SailfishApp::pathTo(QStringLiteral("qml/harbour-soundsail.qml")));
    m_view->showFullScreen();
}

AppController::~AppController()
{
    delete m_view;
}
