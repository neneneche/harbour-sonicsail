import QtQuick 2.0
import Sailfish.Silica 1.0

Image {
    id: coverArt
    property url artworkUrl: ""
    fillMode: Image.PreserveAspectCrop
    asynchronous: true
    source: artworkUrl.toString().length > 0
            ? (artworkUrl.toString( || ""))
            : ""

    Rectangle {
        anchors.fill: parent
        color: Theme.rgba(Theme.highlightColor, 0.12)
        visible: coverArt.status !== Image.Ready
        Icon {
            anchors.centerIn: parent
            source: "image://theme/icon-l-music"
            color: Theme.secondaryHighlightColor
        }
    }
}
