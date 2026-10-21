import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components"

Page {
    id: trackPage

    property var trackData: ({})

    function fmtMs(ms) {
        ms = Number(ms) || 0
        var s = Math.floor(ms / 1000)
        var m = Math.floor(s / 60)
        s = s % 60
        return m + ":" + (s < 10 ? "0" : "") + s
    }
    function fmtCount(n) {
        n = Number(n) || 0
        if (n >= 1000000) return (n / 1000000).toFixed(1) + "M"
        if (n >= 1000) return (n / 1000).toFixed(1) + "k"
        return String(n)
    }

    property var relatedModel: null

    Timer {
        id: relatedTimer
        interval: 50
        onTriggered: {
            if (relatedModel && trackData && trackData.urn)
                relatedModel.loadRelated(trackData.urn)
        }
    }

    Component.onCompleted: {
        relatedModel = api.createTrackModel()
        relatedTimer.start()
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: contentColumn.height + Theme.paddingLarge
        bottomMargin: playerBar.height

        PullDownMenu {
            MenuItem {
                text: qsTr("Open in browser")
                enabled: trackData && trackData.permalinkUrl && trackData.permalinkUrl.length > 0
                onClicked: Qt.openUrlExternally(trackData.permalinkUrl)
            }
            MenuItem {
                text: qsTr("Download")
                enabled: !!trackData && trackData.playable !== false
                onClicked: {
                    downloads.downloadTrack(trackData, settings.saveToMusic)
                    settings.notify(qsTr("Downloading…"))
                }
            }
            MenuItem {
                text: qsTr("Add to queue")
                enabled: !!trackData
                onClicked: {
                    queue.insertNext(trackData)
                    settings.notify(qsTr("Added to queue"))
                }
            }
            MenuItem {
                text: qsTr("Play")
                enabled: !!trackData && trackData.playable !== false
                onClicked: player.startRadio(trackData)
            }
        }

        Column {
            id: contentColumn
            width: parent.width
            spacing: Theme.paddingLarge

            Image {
                id: trackArt
                width: parent.width
                height: width * 0.75
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                source: {
                    var u = trackData && (trackData.artworkUrl || trackData.artwork_url)
                    return u ? (u || "") : ""
                }
                Rectangle {
                    anchors.fill: parent
                    color: Theme.rgba(Theme.highlightColor, 0.12)
                    visible: trackArt.status !== Image.Ready && !(trackData && (trackData.artworkUrl || trackData.artwork_url))
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
                        width: parent.width - trackLikeBtn.width - Theme.paddingMedium
                        text: trackData && trackData.title ? trackData.title : ""
                        color: Theme.primaryColor
                        font.pixelSize: Theme.fontSizeLarge
                        wrapMode: Text.WordWrap
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    IconButton {
                        id: trackLikeBtn
                        width: Theme.itemSizeSmall
                        height: width
                        anchors.verticalCenter: parent.verticalCenter
                        icon.source: (trackData && trackData.userFavorite)
                                    ? "image://theme/icon-favorite-selected"
                                    : "image://theme/icon-favorite"
                        enabled: auth.loggedIn && !!trackData && !!trackData.urn
                        onClicked: {
                            player.toggleLike(trackData)
                            if (trackData.userFavorite !== undefined)
                                trackData.userFavorite = !trackData.userFavorite
                        }
                    }
                }
                Label {
                    width: parent.width
                    text: trackData && trackData.username ? trackData.username : ""
                    color: Theme.highlightColor
                    font.pixelSize: Theme.fontSizeMedium
                }

                Label {
                    text: {
                        if (!trackData) return ""
                        return fmtCount(trackData.playbackCount || 0) + " " + qsTr("plays")
                               + " · " + fmtCount(trackData.likesCount || 0) + " " + qsTr("likes")
                               + " · " + fmtMs(trackData.durationMs || 0)
                    }
                    color: Theme.secondaryColor
                    font.pixelSize: Theme.fontSizeSmall
                }
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Play")
                enabled: !!trackData && trackData.playable !== false
                onClicked: player.startRadio(trackData)
            }

            SectionHeader {
                visible: relatedModel && relatedModel.count > 0
                text: qsTr("Related tracks")
            }

            Repeater {
                model: relatedModel
                delegate: ListItem {
                    contentHeight: Theme.itemSizeMedium
                    menu: Component {
                        ContextMenu {
                            MenuItem {
                                text: qsTr("Open track")
                                onClicked: {
                                    var item = relatedModel.get(index)
                                    if (item && item.title)
                                        pageStack.push(Qt.resolvedUrl("TrackPage.qml"), { trackData: item })
                                }
                            }
                            MenuItem {
                                text: qsTr("Play next")
                                onClicked: {
                                    queue.insertNext(relatedModel.get(index))
                                    settings.notify(qsTr("Queued next"))
                                }
                            }
                            MenuItem {
                                text: model.userFavorite ? qsTr("Remove from likes") : qsTr("Add to likes")
                                enabled: auth.loggedIn
                                onClicked: player.toggleLike(relatedModel.get(index))
                            }
                            MenuItem {
                                text: qsTr("Download")
                                onClicked: {
                                    downloads.downloadTrack(relatedModel.get(index), settings.saveToMusic)
                                    settings.notify(qsTr("Downloading…"))
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
                            source: model.artworkUrl
                                    ? (model.artworkUrl || "")
                                    : ""
                            Rectangle {
                                anchors.fill: parent
                                color: Theme.rgba(Theme.highlightColor, 0.15)
                                visible: model.artworkUrl ? false : true
                            }
                        }

                        Column {
                            width: parent.width - parent.height - Theme.paddingMedium
                            anchors.verticalCenter: parent.verticalCenter
                            Label {
                                width: parent.width
                                text: model.title
                                color: highlighted ? Theme.highlightColor : Theme.primaryColor
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

                    onClicked: {
                        // single press plays radio-style (with its own related queue)
                        var item = relatedModel.get(index)
                        if (item)
                            player.startRadio(item)
                    }
                }
            }
        }

        VerticalScrollDecorator {}
    }

    PlayerBar {
        id: playerBar
    }

    Connections {
        target: downloads
        onFinished: settings.notify(qsTr("Downloaded"))
        onFailed: settings.notify(message)
    }
}
