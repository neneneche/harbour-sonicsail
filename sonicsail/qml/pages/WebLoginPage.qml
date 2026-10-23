import QtQuick 2.0
import Sailfish.Silica 1.0
import Sailfish.WebView 1.0

WebViewPage {
    id: webLoginPage

    WebView {
        id: webView
        anchors {
            top: overlay.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        url: "https://soundcloud.com/signin"
        onUrlChanged: harvest()
    }

    // Slim solid banner: just status text. Navigation is the normal
    // Silica swipe-back; account switching happens on LoginPage before
    // this page is opened.
    Rectangle {
        id: overlay
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
        }
        height: hintColumn.height + Theme.paddingMedium
        color: Theme.highlightDimmerColor

        Column {
            id: hintColumn
            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
                margins: Theme.horizontalPageMargin
            }
            spacing: Theme.paddingSmall

            Label {
                width: parent.width
                text: qsTr("Sign in with your SoundCloud account. The login happens on SoundCloud's own website; Soundsail only picks up your session afterwards. Swipe back to leave.")
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                wrapMode: Text.WordWrap
            }

            Label {
                id: errorLabel
                width: parent.width
                visible: text.length > 0
                text: ""
                color: Theme.errorColor
                font.pixelSize: Theme.fontSizeExtraSmall
                wrapMode: Text.WordWrap
            }

            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                running: auth.busy
                size: BusyIndicatorSize.Small
            }
        }
    }

    // Two harvest channels (same approach as inkitori/soundcloud-desktop, adapted):
    // 1. the app's own Gecko profile cookie database (covers HttpOnly cookies)
    // 2. document.cookie in the page (covers non-HttpOnly cookies)
    function harvest() {
        if (auth.busy || auth.loggedIn)
            return
        auth.importOwnWebProfileCookies()
        webView.runJavaScript("return document.cookie", function(cookies) {
            if (auth.busy || auth.loggedIn || !cookies)
                return
            var m = /(?:^|;\s*)oauth_token=([^;]+)/.exec(cookies)
            if (m && m[1].length > 20)
                auth.loginWithToken(m[1])
        })
    }

    Timer {
        interval: 750
        running: true
        repeat: true
        onTriggered: harvest()
    }

    Component.onCompleted: harvest()

    Connections {
        target: auth
        onLoginSucceeded: webLoginPage.done()
        onLoginFailed: errorLabel.text = message
    }

    function done() {
        errorLabel.text = ""
        // Pop only ourselves; LoginPage beneath closes itself via its
        // onStatusChanged once it becomes the top page again.
        if (pageStack.currentPage === webLoginPage)
            pageStack.pop()
    }
}
