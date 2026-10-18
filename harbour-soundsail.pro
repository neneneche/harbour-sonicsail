TARGET = harbour-soundsail
CONFIG += sailfishapp
QT += quick qml network multimedia sql dbus

PKGCONFIG += sailfishapp

SOURCES += \
    src/main.cpp \
    src/app.cpp \
    src/api/apiclient.cpp \
    src/api/clientidresolver.cpp \
    src/api/jsonparser.cpp \
    src/auth/authmanager.cpp \
    src/auth/tokenstore.cpp \
    src/auth/pkce.cpp \
    src/player/playercontroller.cpp \
    src/player/trackqueue.cpp \
    src/models/trackmodel.cpp \
    src/util/settings.cpp \
    src/util/coverimageprovider.cpp \
    src/util/downloadmanager.cpp \
    src/util/mpriscontroller.cpp

HEADERS += \
    src/app.h \
    src/api/apiclient.h \
    src/api/apitypes.h \
    src/api/clientidresolver.h \
    src/api/jsonparser.h \
    src/auth/authmanager.h \
    src/auth/tokenstore.h \
    src/auth/pkce.h \
    src/player/playercontroller.h \
    src/player/trackqueue.h \
    src/models/trackmodel.h \
    src/util/settings.h \
    src/util/coverimageprovider.h \
    src/util/downloadmanager.h \
    src/util/mpriscontroller.h

OTHER_FILES += \
    harbour-soundsail.desktop \
    rpm/harbour-soundsail.yaml \
    qml/harbour-soundsail.qml \
    qml/cover/CoverPage.qml \
    qml/pages/*.qml \
    qml/components/*.qml \
    qml/js/format.js \
    soundcloud_keys.pri.example

DIST += rpm/harbour-soundsail.yaml

target.path = /usr/bin

qml.files = qml
qml.path = /usr/share/harbour-soundsail

icons86.files = icons/86x86/harbour-soundsail.png
icons86.path = /usr/share/icons/hicolor/86x86/apps
icons108.files = icons/108x108/harbour-soundsail.png
icons108.path = /usr/share/icons/hicolor/108x108/apps
icons128.files = icons/128x128/harbour-soundsail.png
icons128.path = /usr/share/icons/hicolor/128x128/apps
icons172.files = icons/172x172/harbour-soundsail.png
icons172.path = /usr/share/icons/hicolor/172x172/apps

desktop.files = harbour-soundsail.desktop
desktop.path = /usr/share/applications

INSTALLS += target qml icons86 icons108 icons128 icons172 desktop

# Credentials: copy soundcloud_keys.pri.example to soundcloud_keys.pri and fill in.
# The .pri is gitignored. Missing file still compiles (empty defines).
exists(soundcloud_keys.pri) {
    include(soundcloud_keys.pri)
}

isEmpty(SOUNDCLOUD_CLIENT_ID): SOUNDCLOUD_CLIENT_ID =
isEmpty(SOUNDCLOUD_CLIENT_SECRET): SOUNDCLOUD_CLIENT_SECRET =

DEFINES += SOUNDCLOUD_CLIENT_ID=\\\"$${SOUNDCLOUD_CLIENT_ID}\\\"
DEFINES += SOUNDCLOUD_CLIENT_SECRET=\\\"$${SOUNDCLOUD_CLIENT_SECRET}\\\"
