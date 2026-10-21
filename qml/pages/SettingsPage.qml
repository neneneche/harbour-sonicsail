import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: settingsPage

    function save() {
        settings.setRuntimeCredentials(clientIdField.text.trim(),
                                       clientSecretField.text.trim())
        settings.notify(qsTr("Saved"))
        if (clientIdField.text.length > 0)
            auth.ensureToken()
        pageStack.pop()
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader {
                title: qsTr("Settings")
            }

            SectionHeader {
                text: qsTr("Account (optional)")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - Theme.horizontalPageMargin * 2
                text: qsTr("Browse and play work without an account. Sign in only if you want likes, playlists and history.")
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.WordWrap
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: auth.loggedIn ? qsTr("Signed in") : qsTr("Sign in with SoundCloud")
                enabled: !auth.loggedIn && !auth.busy
                onClicked: pageStack.push(Qt.resolvedUrl("LoginPage.qml"))
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: auth.loggedIn
                text: qsTr("Sign out")
                onClicked: {
                    auth.logout()
                    settings.notify(qsTr("Signed out"))
                }
            }

            SectionHeader {
                text: qsTr("Playback")
            }

            ComboBox {
                id: qualityPicker
                label: qsTr("Stream quality")
                currentIndex: {
                    var q = settings.quality
                    if (q === "hls_aac_160") return 0
                    if (q === "hls_mp3_128") return 1
                    return 2
                }
                menu: ContextMenu {
                    MenuItem { text: qsTr("AAC 160"); onClicked: settings.quality = "hls_aac_160" }
                    MenuItem { text: qsTr("MP3 128"); onClicked: settings.quality = "hls_mp3_128" }
                    MenuItem { text: qsTr("Preview / progressive"); onClicked: settings.quality = "preview" }
                }
            }

            SectionHeader {
                text: qsTr("Developer API (optional)")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - Theme.horizontalPageMargin * 2
                text: qsTr("You do not need API keys. If you already have a SoundCloud developer app (paid plan), you can enter its keys below.")
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.WordWrap
            }

            TextField {
                id: clientIdField
                width: parent.width
                label: qsTr("Client ID")
                placeholderText: qsTr("Optional")
                text: settings.clientId || ""
            }

            TextField {
                id: clientSecretField
                width: parent.width
                label: qsTr("Client Secret")
                placeholderText: qsTr("Optional")
                echoMode: TextInput.Password
                text: ""
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Save keys")
                onClicked: save()
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Clear keys")
                visible: settings.hasClientCredentials
                onClicked: {
                    clientIdField.text = ""
                    clientSecretField.text = ""
                    settings.clearCredentials()
                    settings.notify(qsTr("Keys cleared"))
                }
            }

            Item {
                width: 1
                height: Theme.paddingLarge
            }
        }

        VerticalScrollDecorator {}
    }
}
