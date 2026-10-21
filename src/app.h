#pragma once

#include <QObject>
#include <QQuickView>

class ApiClient;
class AuthManager;
class PlayerController;
class TrackQueue;
class Settings;
class CoverImageProvider;

class AppController : public QObject
{
    Q_OBJECT
public:
    explicit AppController(QObject *parent = nullptr);
    ~AppController() override;

private:
    QQuickView *m_view = nullptr;
    ApiClient *m_api = nullptr;
    AuthManager *m_auth = nullptr;
    PlayerController *m_player = nullptr;
    TrackQueue *m_queue = nullptr;
    Settings *m_settings = nullptr;
};
