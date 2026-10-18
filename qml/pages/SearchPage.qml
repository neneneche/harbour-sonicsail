import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components"

Page {
    id: searchPage

    property var searchModel: null

    Component.onCompleted: {
        searchModel = api.createTrackModel()
        if (initialQuery.length > 0) {
            searchField.text = initialQuery
            searchModel.search(initialQuery)
        }
    }

    property string initialQuery: ""

    SilicaListView {
        anchors.fill: parent
        model: searchModel

        header: Column {
            width: searchPage.width
            spacing: Theme.paddingMedium

            PageHeader { title: qsTr("Search") }

            SearchField {
                id: searchField
                width: parent.width
                placeholderText: qsTr("Search tracks")
                EnterKey.enabled: text.length > 0
                EnterKey.iconSource: "image://theme/icon-m-search"
                EnterKey.onClicked: {
                    searchModel.search(text)
                    focus = false
                }
            }
        }

        delegate: TrackDelegate {
            onClicked: {
                var item = searchModel.get(index)
                pageStack.push(Qt.resolvedUrl("TrackPage.qml"), { trackData: item })
            }
        }

        ViewPlaceholder {
            enabled: searchModel && searchModel.count === 0 && !searchModel.busy
                     && searchModel.errorString.length === 0
            text: qsTr("Search for tracks")
        }

        ViewPlaceholder {
            enabled: searchModel && searchModel.errorString.length > 0
            text: searchModel.errorString
        }

        PullDownMenu {
            busy: searchModel && searchModel.busy
            MenuItem {
                text: qsTr("Clear")
                onClicked: {
                    searchField.text = ""
                }
            }
        }

        PushUpMenu {
            MenuItem {
                text: qsTr("Load more")
                enabled: searchModel && searchModel.hasMore
                onClicked: searchModel.loadMore()
            }
        }

        VerticalScrollDecorator {}
    }

    PlayerBar {}
}
