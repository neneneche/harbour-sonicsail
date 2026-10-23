import QtQuick 2.0
import Sailfish.Silica 1.0

CoverBackground {
    id: cover

    Image {
        id: art
        anchors.top: parent.top
        anchors.topMargin: Theme.paddingMedium
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width - Theme.paddingLarge * 2
        height: width
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        source: player.artworkUrl.length > 0
                ? (player.artworkUrl || "")
                : ""
        Rectangle {
            anchors.fill: parent
            color: Theme.rgba(Theme.highlightColor, 0.15)
            visible: art.status !== Image.Ready
            Icon {
                anchors.centerIn: parent
                source: "image://theme/icon-l-music"
            }
        }
    }

    Column {
        anchors.top: art.bottom
        anchors.topMargin: Theme.paddingSmall
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Theme.paddingLarge
        anchors.rightMargin: Theme.paddingLarge
        spacing: Theme.paddingSmall

        Label {
            width: parent.width
            text: player.title.length > 0 ? player.title : qsTr("Nothing playing")
            color: Theme.primaryColor
            font.pixelSize: Theme.fontSizeSmall
            truncationMode: TruncationMode.Fade
            maximumLineCount: 2
            wrapMode: Text.WordWrap
        }
        Label {
            width: parent.width
            visible: player.artist.length > 0
            text: player.artist
            color: Theme.secondaryColor
            font.pixelSize: Theme.fontSizeExtraSmall
            truncationMode: TruncationMode.Fade
        }
    }

    CoverActionList {
        CoverAction {
            iconSource: "image://theme/icon-cover-previous"
            onTriggered: player.previous()
        }
        CoverAction {
            iconSource: player.playing ? "image://theme/icon-cover-pause"
                                       : "image://theme/icon-cover-play"
            onTriggered: player.toggle()
        }
        CoverAction {
            iconSource: "image://theme/icon-cover-next"
            onTriggered: player.next()
        }
    }
}
