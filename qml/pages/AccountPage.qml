import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: accountPage

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingSmall

            PageHeader {
                title: qsTr("Settings")
            }

            DetailItem {
                label: qsTr("Status")
                value: auth.loggedIn ? qsTr("Signed in") : qsTr("Not signed in")
            }

            SectionHeader {
                text: qsTr("Downloads")
            }

            TextSwitch {
                checked: settings.autoDownloadPlayed
                text: qsTr("Auto-download played tracks")
                description: qsTr("Save a track to disk after you listen past 60%")
                onCheckedChanged: settings.autoDownloadPlayed = checked
            }

            TextSwitch {
                checked: settings.saveToMusic
                text: qsTr("Save downloads to Music/Soundsail")
                description: qsTr("When off, files stay in app cache")
                onCheckedChanged: settings.saveToMusic = checked
            }

            SectionHeader {
                text: qsTr("Stream quality")
            }

            ComboBox {
                id: qualityPicker
                label: qsTr("Quality")
                currentIndex: {
                    var q = settings.quality
                    if (q === "hls_aac_160") return 0
                    if (q === "hls_mp3_128") return 1
                    return 2
                }
                menu: ContextMenu {
                    MenuItem {
                        text: qsTr("High — AAC 160")
                        onClicked: settings.quality = "hls_aac_160"
                    }
                    MenuItem {
                        text: qsTr("Medium — MP3 128")
                        onClicked: settings.quality = "hls_mp3_128"
                    }
                    MenuItem {
                        text: qsTr("Low / preview")
                        onClicked: settings.quality = "preview"
                    }
                }
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - Theme.horizontalPageMargin * 2
                text: qsTr("Progressive MP3 is preferred for reliable playback on Sailfish.")
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                wrapMode: Text.WordWrap
            }

            SectionHeader {
                text: qsTr("Comments")
            }

            TextSwitch {
                text: qsTr("Show comments on track pages")
                description: qsTr("Show one comment on the track page; tap it to read more or write your own.")
                checked: settings.showComments
                onCheckedChanged: settings.showComments = checked
            }

            SectionHeader {
                text: qsTr("Account")
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: !auth.loggedIn
                text: qsTr("Sign in")
                onClicked: pageStack.push(Qt.resolvedUrl("LoginPage.qml"))
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: auth.loggedIn
                text: qsTr("Sign out")
                onClicked: {
                    auth.logout()
                    settings.notify(qsTr("Signed out"))
                    // straight back to sign-in so another account can be used
                    pageStack.push(Qt.resolvedUrl("LoginPage.qml"))
                }
            }

            SectionHeader {
                text: qsTr("Experimental")
            }

            // Waveform seek bar — commented out with the feature (not working yet)
            /*
            TextSwitch {
                text: qsTr("Waveform seek bar")
                description: qsTr("Show a SoundCloud waveform on the Now playing page (tap to seek). May not work for all tracks.")
                checked: settings.showWaveform
                onCheckedChanged: settings.showWaveform = checked
            }
            */

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("About")
                onClicked: pageStack.push(Qt.resolvedUrl("AboutPage.qml"))
            }
        }

        VerticalScrollDecorator {}
    }
}
