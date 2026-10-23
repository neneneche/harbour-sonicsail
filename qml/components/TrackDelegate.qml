import QtQuick 2.0
import Sailfish.Silica 1.0

ListItem {
    id: delegate
    contentHeight: Theme.itemSizeMedium

    // true = play entire list from this index (likes / playlist tracks)
    property bool playWholeList: false

    function trackMap() {
        if (typeof tracksModel === "undefined" || !tracksModel)
            return ({})
        return tracksModel.get(index) || ({})
    }

    function isPlaylist() {
        return model.kind === "playlist" || model.kind === "system-playlist"
    }

    menu: Component {
        ContextMenu {
            MenuItem {
                text: qsTr("Open track")
                onClicked: {
                    var item = trackMap()
                    if (item && item.title !== undefined) {
                        if (delegate.isPlaylist())
                            pageStack.push(Qt.resolvedUrl("../pages/PlaylistPage.qml"), { playlistData: item })
                        else
                            pageStack.push(Qt.resolvedUrl("../pages/TrackPage.qml"), { trackData: item })
                    }
                }
            }
            MenuItem {
                text: qsTr("Play next")
                enabled: !delegate.isPlaylist()
                onClicked: {
                    queue.insertNext(trackMap())
                    settings.notify(qsTr("Queued next"))
                }
            }
            MenuItem {
                text: model.userFavorite ? qsTr("Remove from likes") : qsTr("Add to likes")
                enabled: auth.loggedIn && !delegate.isPlaylist()
                onClicked: {
                    var item = trackMap()
                    var want = !model.userFavorite
                    player.toggleLike(item)
                    if (typeof tracksModel !== "undefined" && tracksModel && item && item.urn)
                        tracksModel.setFavorite(item.urn, want)
                    if (item)
                        webWriter.like(String(item.id || ""), want, function(ok) {
                            if (!ok) {
                                player.toggleLike(item)
                                if (typeof tracksModel !== "undefined" && tracksModel && item && item.urn)
                                    tracksModel.setFavorite(item.urn, !want)
                                settings.notify(qsTr("Could not update like"))
                            }
                        })
                }
            }
        }
    }

    Row {
        anchors.fill: parent
        anchors.leftMargin: Theme.horizontalPageMargin
        anchors.rightMargin: Theme.horizontalPageMargin
        spacing: Theme.paddingMedium

        Image {
            width: parent.height - Theme.paddingSmall * 2
            height: width
            anchors.verticalCenter: parent.verticalCenter
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            source: model.artworkUrl ? String(model.artworkUrl) : ""
            Rectangle {
                anchors.fill: parent
                color: Theme.rgba(Theme.highlightColor, 0.15)
                Icon {
                    anchors.centerIn: parent
                    source: "image://theme/icon-s-music"
                }
            }
        }

        Column {
            width: parent.width - parent.height - likeBtn.width - Theme.paddingMedium * 3
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.paddingSmall

            Label {
                width: parent.width
                text: model.title || ""
                color: delegate.highlighted ? Theme.highlightColor : Theme.primaryColor
                font.pixelSize: Theme.fontSizeSmall
                truncationMode: TruncationMode.Fade
            }
            Label {
                width: parent.width
                text: {
                    var parts = []
                    if (model.username) parts.push(model.username)
                    if (delegate.isPlaylist()) parts.push(qsTr("Playlist"))
                    else if (model.durationText) parts.push(model.durationText)
                    return parts.join(" · ")
                }
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                truncationMode: TruncationMode.Fade
            }
        }

        IconButton {
            id: likeBtn
            anchors.verticalCenter: parent.verticalCenter
            visible: !delegate.isPlaylist()
            icon.source: model.userFavorite ? "image://theme/icon-favorite-selected"
                                            : "image://theme/icon-favorite"
            enabled: auth.loggedIn
            onClicked: player.toggleLike(trackMap())
        }
    }

    onClicked: {
        var item = trackMap()
        if (delegate.isPlaylist()) {
            pageStack.push(Qt.resolvedUrl("../pages/PlaylistPage.qml"), { playlistData: item })
            return
        }
        if (playWholeList && typeof tracksModel !== "undefined" && tracksModel)
            player.playFromModel(tracksModel, index)
        else
            player.startRadio(item)
    }
}
