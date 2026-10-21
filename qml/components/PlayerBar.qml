import QtQuick 2.0
import Sailfish.Silica 1.0

Rectangle {
    id: playerBar
    property bool dismissed: false
    property bool hideOnPlayer: false

    width: parent ? parent.width : Screen.width
    height: visible ? Theme.itemSizeSmall : 0
    visible: player.title.length > 0 && !dismissed && !hideOnPlayer
    color: Theme.rgba(Theme.highlightDimmerColor, 0.85)
    z: 100

    anchors.bottom: parent.bottom

    function dismiss() {
        dismissed = true
        player.stop()
    }

    Connections {
        target: player
        onTrackChanged: playerBar.dismissed = false
    }

    Image {
        id: art
        anchors.left: parent.left
        anchors.leftMargin: Theme.horizontalPageMargin
        anchors.verticalCenter: parent.verticalCenter
        width: parent.height - Theme.paddingSmall
        height: width
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        source: player.artworkUrl.length > 0 ? player.artworkUrl : ""
    }

    Column {
        anchors.left: art.right
        anchors.leftMargin: Theme.paddingMedium
        anchors.right: playBtn.left
        anchors.rightMargin: Theme.paddingMedium
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2

        Label {
            width: parent.width
            text: player.title
            color: Theme.primaryColor
            font.pixelSize: Theme.fontSizeSmall
            truncationMode: TruncationMode.Fade
        }
        Label {
            width: parent.width
            text: player.artist
            color: Theme.secondaryColor
            font.pixelSize: Theme.fontSizeExtraSmall
            truncationMode: TruncationMode.Fade
        }
    }

    IconButton {
        id: playBtn
        anchors.right: parent.right
        anchors.rightMargin: Theme.horizontalPageMargin
        anchors.verticalCenter: parent.verticalCenter
        icon.source: player.playing ? "image://theme/icon-m-pause"
                                    : "image://theme/icon-m-play"
        onClicked: player.toggle()
    }

    ProgressBar {
        anchors.bottom: parent.bottom
        width: parent.width
        height: Theme.paddingSmall
        indeterminate: false
        minimumValue: 0
        maximumValue: Math.max(player.duration, 1)
        value: player.position
    }

    MouseArea {
        anchors.fill: parent
        anchors.rightMargin: Theme.itemSizeMedium + Theme.paddingMedium
        property real pressY: 0
        onPressed: pressY = mouse.y
        onClicked: {
            if (Math.abs(mouse.y - pressY) < 12)
                pageStack.push(Qt.resolvedUrl("../pages/PlayerPage.qml"))
        }
        onPositionChanged: {
            if (pressed && (mouse.y - pressY) > playerBar.height * 0.6)
                playerBar.dismiss()
        }
    }
}
