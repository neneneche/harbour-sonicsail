import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components"

Page {
    id: likesPage

    property var likesModel: null

    Component.onCompleted: {
        likesModel = api.createTrackModel()
        reloadTimer.start()
    }

    // always refresh when this page becomes active
    onStatusChanged: {
        if (status === PageStatus.Active && likesModel)
            likesModel.loadLikes()
    }

    Timer {
        id: reloadTimer
        interval: 50
        onTriggered: {
            if (likesModel)
                likesModel.loadLikes()
        }
    }

    SilicaListView {
        anchors.fill: parent
        model: likesModel

        header: Column {
            width: parent.width
            PageHeader { title: qsTr("Liked tracks") }
            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Play all")
                enabled: likesModel && likesModel.count > 0
                onClicked: player.playFromModel(likesModel, 0)
            }
        }

        delegate: TrackDelegate {
            playWholeList: true
        }

        ViewPlaceholder {
            enabled: likesModel && likesModel.count === 0 && !likesModel.busy
                     && likesModel.errorString.length === 0
            text: qsTr("No liked tracks")
        }

        ViewPlaceholder {
            enabled: likesModel && likesModel.errorString.length > 0
            text: likesModel.errorString
        }

        PullDownMenu {
            busy: likesModel && likesModel.busy
            MenuItem {
                text: qsTr("Reload")
                onClicked: likesModel.loadLikes()
            }
        }

        PushUpMenu {
            MenuItem {
                text: qsTr("Load more")
                enabled: likesModel && likesModel.hasMore
                onClicked: likesModel.loadMore()
            }
        }

        VerticalScrollDecorator {}
    }

    PlayerBar {}
}
