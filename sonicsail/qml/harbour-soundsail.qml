import QtQuick 2.0
import Sailfish.Silica 1.0
import "pages"
import "cover"
import "components"

ApplicationWindow {
    id: window

    initialPage: Qt.resolvedUrl("pages/MainPage.qml")
    cover: Qt.resolvedUrl("cover/CoverPage.qml")
    allowedOrientations: defaultAllowedOrientations

    // hidden in-page browser used for writes (likes/comments/… — the bot
    // protection rejects plain HTTP writes)
    WebWriter {
        id: webWriter
    }

    Notice {
        id: notice
    }

    Connections {
        target: settings
        onNotifyRequested: {
            notice.text = message
            notice.show()
        }
    }

    Connections {
        target: auth
        onSessionExpired: {
            settings.notify(qsTr("Session expired — sign in again"))
            // straight to the sign-in page so a new login can be made
            if (pageStack.depth === 1)
                pageStack.push(Qt.resolvedUrl("pages/LoginPage.qml"))
        }
    }
}
