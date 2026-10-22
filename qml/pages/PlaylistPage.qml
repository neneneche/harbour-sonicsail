import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components"

Page {
    id: playlistPage

    property var playlistData: ({})
    property var tracksModel: null

    Component.onCompleted: {
        tracksModel = api.createTrackModel()
        loadTimer.start()
    }

    Timer {
        id: loadTimer
        interval: 50
        onTriggered: {
            if (!tracksModel || !playlistData)
                return
            // prefer the full URN (system playlists are URN-addressed)
            var ref = ""
            if (playlistData.urn && String(playlistData.urn).length > 0)
                ref = String(playlistData.urn)
            else if (playlistData.id)
                ref = String(playlistData.id)
            if (!ref) {
                tracksModel.clearItems()
                return
            }
            tracksModel.loadPlaylistTracks(ref)
        }
    }

    SilicaListView {
        id: list
        anchors.fill: parent
        model: tracksModel
        bottomMargin: playerBar.height

        header: Column {
            width: playlistPage.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: playlistData.title || qsTr("Playlist")
            }

            Image {
                x: Theme.horizontalPageMargin
                width: parent.width - Theme.horizontalPageMargin * 2
                height: width * 0.5
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                source: playlistData.artworkUrl ? String(playlistData.artworkUrl) : ""
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - Theme.horizontalPageMargin * 2
                text: playlistData.username || ""
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeMedium
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Play all")
                enabled: tracksModel && tracksModel.count > 0
                onClicked: player.playFromModel(tracksModel, 0)
            }
        }

        delegate: TrackDelegate {
            playWholeList: true
        }

        ViewPlaceholder {
            enabled: tracksModel && tracksModel.count === 0 && !tracksModel.busy
            text: qsTr("No tracks")
        }

        VerticalScrollDecorator {}
    }

    PlayerBar {
        id: playerBar
    }
}
