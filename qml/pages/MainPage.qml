import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components"

Page {
    id: page

    property var tracksModel: null
    property int tab: 0 // 0 playlists, 1 search, 2 likes

    Component.onCompleted: {
        tracksModel = api.createTrackModel()
        loadTimer.start()
    }

    Timer {
        id: loadTimer
        interval: 80
        onTriggered: reloadTab()
    }

    function reloadTab() {
        if (!tracksModel)
            return
        if (tab === 0)
            tracksModel.loadUserPlaylists()
        else if (tab === 2)
            tracksModel.loadLikes()
        else if (tab === 1 && searchField.text.length > 2)
            tracksModel.search(searchField.text)
    }

    function showTab(i) {
        tab = i
        if (!tracksModel)
            return
        if (i === 1) {
            tracksModel.clearItems()
            return
        }
        reloadTab()
    }

    onStatusChanged: {
        if (status === PageStatus.Active)
            reloadTab()
    }

    Timer {
        id: searchDebounce
        interval: 500
        onTriggered: {
            if (tab === 1 && searchField.text.length > 2 && tracksModel)
                tracksModel.search(searchField.text)
        }
    }

    SilicaGridView {
        id: grid
        anchors.fill: parent
        visible: tab === 0
        model: tab === 0 ? tracksModel : null
        bottomMargin: playerBar.height
        clip: true

        header: Column {
            width: grid.width
            PageHeader { title: qsTr("Playlists") }
            TabRow {
                currentTab: tab
                onTabSelected: showTab(index)
            }
        }

        cellWidth: grid.width / 2
        cellHeight: cellWidth

        delegate: PlaylistTile {
            title: model.title || ""
            artworkUrl: model.artworkUrl || ""
            username: model.username || ""
            payload: {
                if (!tracksModel)
                    return ({})
                return tracksModel.get(index) || ({})
            }
        }

        ViewPlaceholder {
            enabled: grid.count === 0 && tracksModel && !tracksModel.busy
            text: qsTr("No playlists")
            hintText: auth.loggedIn ? "" : qsTr("Sign in to see your playlists")
        }

        PullDownMenu {
            MenuItem {
                text: qsTr("Settings")
                onClicked: pageStack.push(Qt.resolvedUrl(auth.loggedIn ? "AccountPage.qml"
                                                                        : "LoginPage.qml"))
            }
            MenuItem {
                text: qsTr("Now playing")
                visible: player.title.length > 0
                onClicked: pageStack.push(Qt.resolvedUrl("PlayerPage.qml"))
            }
            MenuItem {
                text: qsTr("Reload")
                onClicked: reloadTab()
            }
        }

        VerticalScrollDecorator {}
    }

    SilicaListView {
        id: listView
        anchors.fill: parent
        model: tab === 1 || tab === 2 ? tracksModel : null
        visible: tab === 1 || tab === 2
        bottomMargin: playerBar.height
        clip: true

        header: Column {
            width: listView.width
            PageHeader {
                title: tab === 2 ? qsTr("Likes") : qsTr("Search")
            }
            TabRow {
                currentTab: tab
                onTabSelected: showTab(index)
            }

            SearchField {
                id: searchField
                width: parent.width
                height: Theme.itemSizeMedium
                opacity: tab === 1 ? 1 : 0
                enabled: tab === 1
                placeholderText: qsTr("Search tracks")
                EnterKey.enabled: text.length > 2
                EnterKey.iconSource: "image://theme/icon-m-search"
                onTextChanged: {
                    if (text.length > 2)
                        searchDebounce.restart()
                }
                EnterKey.onClicked: {
                    tracksModel.search(text)
                    focus = false
                }
            }
        }

        delegate: TrackDelegate {
            playWholeList: tab === 2
        }

        ViewPlaceholder {
            enabled: tab === 1 && listView.count === 0 && tracksModel && !tracksModel.busy
            text: qsTr("Search field is up :)")
        }

        ViewPlaceholder {
            enabled: tab === 2 && listView.count === 0 && tracksModel && !tracksModel.busy
                     && tracksModel.errorString.length === 0
            text: auth.loggedIn ? qsTr("No liked tracks") : qsTr("Sign in to see likes")
        }

        PullDownMenu {
            MenuItem {
                text: qsTr("Settings")
                onClicked: pageStack.push(Qt.resolvedUrl(auth.loggedIn ? "AccountPage.qml"
                                                                        : "LoginPage.qml"))
            }
            MenuItem {
                text: qsTr("Now playing")
                visible: player.title.length > 0
                onClicked: pageStack.push(Qt.resolvedUrl("PlayerPage.qml"))
            }
            MenuItem {
                text: qsTr("Reload")
                onClicked: reloadTab()
            }
        }

        VerticalScrollDecorator {}
    }

    PlayerBar {
        id: playerBar
    }
}
