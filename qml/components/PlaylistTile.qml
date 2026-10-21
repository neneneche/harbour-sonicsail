import QtQuick 2.0
import Sailfish.Silica 1.0

BackgroundItem {
    id: tile
    property string title: ""
    property string artworkUrl: ""
    property string username: ""
    property string kind: ""
    property var payload: ({})

    width: (Screen.width - Theme.horizontalPageMargin * 2 - Theme.paddingMedium) / 2
    height: width

    onClicked: {
        if (kind === "track")
            pageStack.push(Qt.resolvedUrl("../pages/TrackPage.qml"), { trackData: payload })
        else
            pageStack.push(Qt.resolvedUrl("../pages/PlaylistPage.qml"), { playlistData: payload })
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.paddingSmall
        color: Theme.rgba(Theme.highlightColor, 0.12)
        clip: true

        Image {
            anchors.fill: parent
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            source: tile.artworkUrl ? tile.artworkUrl : ""
        }

        // gradient scrim for title
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: parent.height * 0.45
            gradient: Gradient {
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 1.0; color: "#CC000000" }
            }
        }

        Label {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: Theme.paddingMedium
            text: tile.title
            color: "white"
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            truncationMode: TruncationMode.Fade
        }
    }

    // highlight when pressed
    Rectangle {
        anchors.fill: parent
        radius: Theme.paddingSmall
        color: Theme.rgba(Theme.highlightColor, 0.25)
        visible: tile.highlighted
    }
}
