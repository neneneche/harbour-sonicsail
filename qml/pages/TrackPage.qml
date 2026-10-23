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
    property var commentsModel: []
    property string commentsTag: ""
    property bool commentsExpanded: false
    property bool liked: false
    property bool reposted: false
    property bool following: false
    property string pendingFollow: ""

    function detail(msg, info) {
        var d = String(info === undefined || info === null ? "" : info).substring(0, 60)
        return d.length > 0 ? msg + " (" + d + ")" : msg
    }

    function fmtCommentDate(iso) {
        if (!iso)
            return ""
        return String(iso).replace("T", " ").substring(0, 16)
    }

    function loadComments() {
        commentsModel = []
        commentsExpanded = false
        if (!settings.showComments)
            return
        var id = trackId()
        if (!id)
            return
        commentsTag = "comments-" + id
        api.get("/tracks/" + id + "/comments",
                { limit: 20, linked_partitioning: 1, threaded: 1 },
                commentsTag)
    }

    function postComment() {
        var id = trackId()
        if (!id || commentField.text.length === 0)
            return
        webWriter.comment(id, commentField.text, function(ok, info) {
            if (ok) {
                settings.notify(qsTr("Comment posted"))
                trackPage.loadComments()
            } else {
                var detail = String(info === undefined || info === null ? "" : info).substring(0, 80)
                settings.notify(detail.length > 0
                                ? qsTr("Could not post comment (%1)").arg(detail)
                                : qsTr("Could not post comment"))
            }
        })
        commentField.text = ""
    }

    function trackId() {
        if (!trackData)
            return ""
        if (trackData.urn) {
            var u = String(trackData.urn)
            return u.substring(u.lastIndexOf(":") + 1)
        }
        return trackData.id ? String(trackData.id) : ""
    }

    function userFollowId() {
        // flat key first (same shape on every path), then nested/urn fallbacks
        var id = String(trackData ? (trackData.userId !== undefined ? trackData.userId : "") : "")
        var u = trackData && trackData.user ? trackData.user : {}
        if (!id || id === "0" || id === "undefined") {
            if (u && u.id !== undefined)
                id = String(u.id)
        }
        if (!id || id === "0" || id === "undefined") {
            var urn = String(u && u.urn ? u.urn : "")
            var c = urn.lastIndexOf(":")
            if (c >= 0)
                id = urn.substring(c + 1)
        }
        return id && id !== "undefined" ? id : "0"
    }

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
        liked = !!(trackData && trackData.userFavorite)
        relatedTimer.start()
        loadComments()
    }

    Connections {
        target: api
        onListReceived: {
            if (tag === trackPage.commentsTag)
                commentsModel = items
        }
        onSuccess: {
            if (tag !== "follow-resolve")
                return
            // `data` is a normalized track map (same shape as list items)
            if (data && data.title !== undefined)
                trackData = data
            var fid = String(data && data.userId !== undefined ? data.userId : "0")
            if (fid === "0" && data && data.user && data.user.id)
                fid = String(data.user.id)
            if (!fid || fid === "0") {
                trackPage.pendingFollow = ""
                settings.notify(qsTr("Could not update follow (no user id)"))
                return
            }
            var want = trackPage.pendingFollow === "follow"
            trackPage.pendingFollow = ""
            trackPage.following = want
            webWriter.follow(fid, want, function(ok, info) {
                if (!ok) {
                    trackPage.following = !want
                    settings.notify(detail(qsTr("Could not update follow"), info))
                }
            })
        }
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
                        icon.source: trackPage.liked
                                    ? "image://theme/icon-favorite-selected"
                                    : "image://theme/icon-favorite"
                        enabled: auth.loggedIn && !!trackData && !!trackData.urn
                        onClicked: {
                            trackPage.liked = !trackPage.liked
                            player.toggleLike(trackData)
                            webWriter.like(trackPage.trackId(), trackPage.liked, function(ok, info) {
                                if (!ok) {
                                    trackPage.liked = !trackPage.liked
                                    player.toggleLike(trackData)
                                    settings.notify(detail(qsTr("Could not update like"), info))
                                }
                            })
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

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Theme.paddingMedium

                Button {
                    text: trackPage.reposted ? qsTr("Undo repost") : qsTr("Repost")
                    enabled: auth.loggedIn && !!trackData && !!trackData.urn
                    onClicked: {
                        trackPage.reposted = !trackPage.reposted
                        webWriter.repost(trackPage.trackId(), trackPage.reposted, function(ok) {
                            if (!ok) {
                                trackPage.reposted = !trackPage.reposted
                                settings.notify(qsTr("Could not update repost"))
                            }
                        })
                    }
                }
                Button {
                    text: trackPage.following ? qsTr("Unfollow") : qsTr("Follow")
                    enabled: auth.loggedIn && !!trackData
                    onClicked: {
                        var fid = trackPage.userFollowId()
                        if (!fid || fid === "0") {
                            // resolve the track through the same normalizer the
                            // list maps use (api client returns trackMap output)
                            pendingFollow = trackPage.following ? "unfollow" : "follow"
                            api.get("/tracks/" + trackId(), {}, "follow-resolve")
                            return
                        }
                        trackPage.following = !trackPage.following
                        webWriter.follow(fid, trackPage.following, function(ok, info) {
                            if (!ok) {
                                trackPage.following = !trackPage.following
                                settings.notify(detail(qsTr("Could not update follow"), info))
                            }
                        })
                    }
                }
            }

            // Comments: one preview comment; tapping it reveals the rest
            // plus the write field (pure Silica — just visibility toggling)
            SectionHeader {
                visible: settings.showComments && commentsModel.length > 0
                text: qsTr("Comments")
            }

            ListItem {
                id: firstComment
                visible: settings.showComments && commentsModel.length > 0
                contentHeight: firstCommentCol.height + Theme.paddingMedium
                onClicked: commentsExpanded = !commentsExpanded

                Column {
                    id: firstCommentCol
                    x: Theme.horizontalPageMargin
                    width: parent.width - Theme.horizontalPageMargin * 2
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: Theme.paddingSmall

                    Label {
                        width: parent.width
                        text: (commentsModel[0].username || "") + "  ·  "
                              + trackPage.fmtCommentDate(commentsModel[0].createdAt)
                        color: Theme.highlightColor
                        font.pixelSize: Theme.fontSizeExtraSmall
                        truncationMode: TruncationMode.Fade
                    }
                    Label {
                        width: parent.width
                        text: commentsModel[0].body || ""
                        color: Theme.primaryColor
                        font.pixelSize: Theme.fontSizeSmall
                        wrapMode: Text.WordWrap
                        maximumLineCount: commentsExpanded ? 100 : 2
                        truncationMode: TruncationMode.Fade
                    }
                    Label {
                        width: parent.width
                        text: commentsExpanded
                              ? qsTr("Show less")
                              : (commentsModel.length > 1
                                 ? qsTr("Tap to show %1 comments").arg(commentsModel.length)
                                 : qsTr("Tap to write a comment"))
                        color: Theme.secondaryHighlightColor
                        font.pixelSize: Theme.fontSizeExtraSmall
                    }
                }
            }

            Column {
                width: parent.width
                visible: settings.showComments && commentsExpanded

                Repeater {
                    model: commentsModel
                    delegate: ListItem {
                        visible: index > 0
                        contentHeight: commentCol.height + Theme.paddingMedium

                        Column {
                            id: commentCol
                            x: Theme.horizontalPageMargin
                            width: parent.width - Theme.horizontalPageMargin * 2
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: Theme.paddingSmall

                            Label {
                                width: parent.width
                                text: (modelData.username || "") + "  ·  "
                                      + trackPage.fmtCommentDate(modelData.createdAt)
                                color: Theme.highlightColor
                                font.pixelSize: Theme.fontSizeExtraSmall
                                truncationMode: TruncationMode.Fade
                            }
                            Label {
                                width: parent.width
                                text: modelData.body || ""
                                color: Theme.primaryColor
                                font.pixelSize: Theme.fontSizeSmall
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }

                TextField {
                    id: commentField
                    x: Theme.horizontalPageMargin
                    width: parent.width - Theme.horizontalPageMargin * 2
                    visible: auth.loggedIn
                    label: qsTr("Add a comment")
                    placeholderText: qsTr("Write a comment…")
                    EnterKey.enabled: text.length > 0
                    EnterKey.iconSource: "image://theme/icon-m-enter-accept"
                    EnterKey.onClicked: {
                        postComment()
                        focus = false
                    }
                }
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
                                onClicked: {
                                    var item = relatedModel.get(index)
                                    var want = !model.userFavorite
                                    player.toggleLike(item)
                                    relatedModel.setFavorite(item.urn, want)
                                    webWriter.like(String(item.id), want, function(ok) {
                                        if (!ok) {
                                            relatedModel.setFavorite(item.urn, !want)
                                            settings.notify(qsTr("Could not update like"))
                                        }
                                    })
                                }
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
