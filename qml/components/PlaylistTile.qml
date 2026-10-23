import QtQuick 2.0
import Sailfish.Silica 1.0

BackgroundItem {
    id: tile
    property string title: ""
    property string artworkUrl: ""
    property string username: ""
    property string kind: ""
    property var payload: ({})

    // artwork-less mixes get a 2x2 mosaic of the first tracks' covers
    property var mosaicArt: {
        if (artworkUrl && artworkUrl.length > 0)
            return []
        if (payload && payload.artworkCandidates && payload.artworkCandidates.length > 0)
            return payload.artworkCandidates
        var out = []
        var tracks = payload && payload.tracks ? payload.tracks : []
        for (var i = 0; i < tracks.length && out.length < 4; i++) {
            var t = tracks[i]
            var u = t && (t.artworkUrl || t.artwork_url)
            if (u)
                out.push(u)
        }
        return out
    }

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
            visible: tile.mosaicArt.length === 0
            source: tile.artworkUrl ? tile.artworkUrl : ""
        }

        // 2x2 mosaic when there is no single artwork
        Grid {
            anchors.fill: parent
            columns: 2
            rows: 2
            visible: tile.mosaicArt.length > 0
            Repeater {
                model: 4
                Item {
                    width: tile.width / 2
                    height: tile.height / 2
                    Image {
                        anchors.fill: parent
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        source: index < tile.mosaicArt.length ? tile.mosaicArt[index] : ""
                    }
                    Rectangle {
                        anchors.fill: parent
                        color: Theme.rgba(Theme.highlightColor, 0.10)
                        visible: !(index < tile.mosaicArt.length)
                    }
                }
            }
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
