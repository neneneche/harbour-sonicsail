import QtQuick 2.0
import Sailfish.Silica 1.0

Row {
    id: tabRow
    property int currentTab: 0
    // when false, the account-only tabs (Home/Likes/Playlists) are dimmed
    // and inert — signed-out users only get Search
    property bool accountTabsEnabled: true
    signal tabSelected(int index)

    x: Theme.horizontalPageMargin
    width: parent ? parent.width - Theme.horizontalPageMargin * 2 : Screen.width
    height: Theme.itemSizeSmall

    Repeater {
        model: [
            { t: qsTr("Home"), i: 0, account: true },
            { t: qsTr("Search"), i: 1, account: false },
            { t: qsTr("Likes"), i: 2, account: true },
            { t: qsTr("Playlists"), i: 3, account: true }
        ]
        BackgroundItem {
            property bool tabEnabled: !modelData.account || tabRow.accountTabsEnabled
            width: tabRow.width / 4
            height: tabRow.height
            enabled: tabEnabled
            opacity: tabEnabled ? 1.0 : 0.4
            onClicked: tabRow.tabSelected(modelData.i)
            Label {
                anchors.centerIn: parent
                text: modelData.t
                color: {
                    if (tabRow.currentTab === modelData.i)
                        return Theme.highlightColor
                    return parent.tabEnabled ? Theme.secondaryColor : Theme.secondaryHighlightColor
                }
                font.bold: tabRow.currentTab === modelData.i
                font.pixelSize: Theme.fontSizeSmall
            }
        }
    }
}
