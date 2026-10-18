import QtQuick 2.0
import Sailfish.Silica 1.0
import "pages"
import "cover"

ApplicationWindow {
    id: window

    initialPage: Qt.resolvedUrl("pages/MainPage.qml")
    cover: Qt.resolvedUrl("cover/CoverPage.qml")
    allowedOrientations: defaultAllowedOrientations

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
}
