import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components"

Page {
    id: playerPage
    objectName: "PlayerPage"

    function fmtMs(ms) {
        ms = Number(ms) || 0
        var s = Math.floor(ms / 1000)
        var m = Math.floor(s / 60)
        s = s % 60
        return m + ":" + (s < 10 ? "0" : "") + s
    }

    SilicaFlickable {
        id: flick
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge
        bottomMargin: playerBar.height

        PullDownMenu {
            MenuItem {
                text: qsTr("Download")
                enabled: player.permalinkUrl.length > 0 || player.title.length > 0
                onClicked: {
                    downloads.downloadTrack(queue.currentTrack, settings.saveToMusic)
                    settings.notify(qsTr("Downloading…"))
                }
            }
            MenuItem {
                text: qsTr("Open in browser")
                enabled: player.permalinkUrl.length > 0
                onClicked: Qt.openUrlExternally(player.permalinkUrl)
            }
            MenuItem {
                text: qsTr("Copy link")
                enabled: player.permalinkUrl.length > 0
                onClicked: {
                    Clipboard.text = player.permalinkUrl
                    settings.notify(qsTr("Link copied"))
                }
            }
        }

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader {
                title: qsTr("Now playing")
            }

            Image {
                id: bigArt
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width - Theme.paddingLarge * 2
                height: width
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                source: player.artworkUrl.length > 0
                        ? (player.artworkUrl || "")
                        : ""
                // only show placeholder when there is no artwork yet
                Rectangle {
                    anchors.fill: parent
                    color: Theme.rgba(Theme.highlightColor, 0.12)
                    visible: bigArt.status !== Image.Ready && player.artworkUrl.length === 0
                }
            }

            Column {
                x: Theme.horizontalPageMargin
                width: parent.width - Theme.horizontalPageMargin * 2
                spacing: Theme.paddingSmall

                Row {
                    width: parent.width
                    spacing: Theme.paddingMedium

                    Label {
                        width: parent.width - likeBtn.width - Theme.paddingMedium
                        text: player.title.length > 0 ? player.title : qsTr("Nothing playing")
                        color: Theme.primaryColor
                        font.pixelSize: Theme.fontSizeLarge
                        wrapMode: Text.WordWrap
                        maximumLineCount: 3
                        truncationMode: TruncationMode.Fade
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    IconButton {
                        id: likeBtn
                        width: Theme.itemSizeSmall
                        height: width
                        anchors.verticalCenter: parent.verticalCenter
                        visible: player.title.length > 0
                        icon.source: player.liked
                                    ? "image://theme/icon-favorite-selected"
                                    : "image://theme/icon-favorite"
                        enabled: auth.loggedIn && player.title.length > 0
                        onClicked: player.toggleCurrentLike()
                    }
                }
                Label {
                    width: parent.width
                    text: player.artist
                    visible: player.artist.length > 0
                    color: Theme.highlightColor
                    font.pixelSize: Theme.fontSizeMedium
                }
                Label {
                    width: parent.width
                    visible: player.statusMessage.length > 0
                    text: player.statusMessage
                    color: Theme.secondaryHighlightColor
                    font.pixelSize: Theme.fontSizeSmall
                }
            }

            Slider {
                id: progressSlider
                width: parent.width
                minimumValue: 0
                maximumValue: Math.max(player.duration, 1)
                // NO value: player.position — drag would destroy the binding
                enabled: player.duration > 0
                onReleased: player.seek(Math.round(value))
                label: fmtMs(player.position) + " / " + fmtMs(player.duration)
            }
            Connections {
                target: player
                onPositionChanged: {
                    if (!progressSlider.pressed)
                        progressSlider.value = player.position
                }
                onTrackChanged: progressSlider.value = 0
            }

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Theme.paddingMedium

                IconButton {
                    icon.source: player.shuffle ? "image://theme/icon-m-shuffle"
                                                : "image://theme/icon-m-shuffle"
                    highlighted: player.shuffle
                    onClicked: player.shuffle = !player.shuffle
                }

                IconButton {
                    icon.source: "image://theme/icon-m-previous"
                    onClicked: player.previous()
                }
                IconButton {
                    width: Theme.itemSizeLarge
                    height: width
                    icon.source: player.playing ? "image://theme/icon-l-pause"
                                                : "image://theme/icon-l-play"
                    onClicked: player.toggle()
                }
                IconButton {
                    icon.source: "image://theme/icon-m-next"
                    onClicked: player.next()
                }

                IconButton {
                    icon.source: player.repeatMode === 2 ? "image://theme/icon-m-repeat"
                                                         : "image://theme/icon-m-repeat"
                    highlighted: player.repeatMode > 0
                    onClicked: player.repeatMode = (player.repeatMode + 1) % 3
                }
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: player.repeatMode > 0
                text: player.repeatMode === 1 ? qsTr("Repeat all") : qsTr("Repeat one")
                color: Theme.secondaryHighlightColor
                font.pixelSize: Theme.fontSizeExtraSmall
            }

            SectionHeader {
                text: qsTr("Queue")
                visible: queue.count > 0
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: queue.count > 0
                text: qsTr("Clear queue")
                onClicked: {
                    queue.clear()
                    player.stop()
                }
            }

            // Column+Repeater so every queue entry is laid out (nested ListView
            // with height:contentHeight can stop at a partial range)
            Column {
                width: parent.width
                Repeater {
                    model: queue
                    delegate: ListItem {
                        contentHeight: Theme.itemSizeSmall
                        menu: Component {
                            ContextMenu {
                                MenuItem {
                                    text: qsTr("Remove from queue")
                                    onClicked: queue.removeAt(index)
                                }
                                MenuItem {
                                    text: qsTr("Open track")
                                    onClicked: {
                                        var item = queue.get(index)
                                        if (item && item.title)
                                            pageStack.push(Qt.resolvedUrl("TrackPage.qml"), { trackData: item })
                                    }
                                }
                            }
                        }

                        Row {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.horizontalPageMargin
                            anchors.rightMargin: Theme.horizontalPageMargin
                            spacing: Theme.paddingMedium

                            Label {
                                text: (index + 1) + "."
                                color: Theme.secondaryColor
                                anchors.verticalCenter: parent.verticalCenter
                                font.pixelSize: Theme.fontSizeSmall
                            }
                            Column {
                                width: parent.width - Theme.itemSizeSmall
                                anchors.verticalCenter: parent.verticalCenter
                                Label {
                                    width: parent.width
                                    text: model.title
                                    color: model.isCurrent ? Theme.highlightColor : Theme.primaryColor
                                    font.pixelSize: Theme.fontSizeSmall
                                    truncationMode: TruncationMode.Fade
                                }
                                Label {
                                    width: parent.width
                                    text: model.username
                                    color: Theme.secondaryColor
                                    font.pixelSize: Theme.fontSizeExtraSmall
                                    truncationMode: TruncationMode.Fade
                                }
                            }
                        }
                        onClicked: player.playIndex(index)
                    }
                }
            }

            Item {
                width: 1
                height: Theme.paddingLarge
            }
        }

        VerticalScrollDecorator {}
    }

    PlayerBar {
        id: playerBar
        hideOnPlayer: true
    }

    Connections {
        target: downloads
        onFinished: settings.notify(qsTr("Downloaded"))
        onFailed: settings.notify(message)
    }
}
