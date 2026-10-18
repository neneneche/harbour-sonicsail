import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components"

Page {
    id: historyPage

    property var historyModel: null

    Component.onCompleted: {
        historyModel = api.createTrackModel()
        historyModel.loadHistory()
    }

    SilicaListView {
        anchors.fill: parent
        model: historyModel

        header: PageHeader { title: qsTr("Recently played") }

        delegate: TrackDelegate {
            onClicked: {
                var item = historyModel.get(index)
                pageStack.push(Qt.resolvedUrl("TrackPage.qml"), { trackData: item })
            }
        }

        ViewPlaceholder {
            enabled: historyModel && historyModel.count === 0 && !historyModel.busy
                     && historyModel.errorString.length === 0
            text: qsTr("Nothing played yet")
        }

        ViewPlaceholder {
            enabled: historyModel && historyModel.errorString.length > 0
            text: historyModel.errorString
        }

        PullDownMenu {
            busy: historyModel && historyModel.busy
            MenuItem {
                text: qsTr("Reload")
                onClicked: historyModel.loadHistory()
            }
        }

        VerticalScrollDecorator {}
    }

    PlayerBar {}
}
