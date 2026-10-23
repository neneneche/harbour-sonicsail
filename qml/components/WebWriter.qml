import QtQuick 2.0
import Sailfish.Silica 1.0
import Sailfish.WebView 1.0

// Writes (like / repost / follow) must come from a real browser context:
// SoundCloud's bot protection rejects plain HTTP writes (the same reason
// inkitori/soundcloud-desktop performs writes through a hidden WebView).
//
// The WebView is LAZY: it is not created at app startup (a second WebView
// running alongside the sign-in page glitches the embedded engine) — only
// on the first actual write, after the sign-in browser is closed.
Item {
    id: webWriter
    width: 8
    height: 8
    opacity: 0

    property var pending: []

    Loader {
        id: webViewLoader
        anchors.fill: parent
        active: false
        sourceComponent: Component {
            WebView {
                active: true
                url: "https://soundcloud.com"
                onLoadedChanged: {
                    if (loaded)
                        webWriter.flush()
                }
            }
        }
    }

    function ensureWebView() {
        if (!webViewLoader.active)
            webViewLoader.active = true
        return webViewLoader.item
    }

    // ---- public helpers -------------------------------------------------
    function like(trackId, on, cb) {
        write(on ? "PUT" : "DELETE",
              "https://api-v2.soundcloud.com/users/" + String(auth.userId || "0") + "/track_likes/" + trackId,
              "", cb)
    }

    function repost(trackId, on, cb) {
        write(on ? "PUT" : "DELETE",
              "https://api-v2.soundcloud.com/me/track_reposts/" + trackId,
              "", cb)
    }

    function follow(userId, on, cb) {
        write(on ? "POST" : "DELETE",
              "https://api-v2.soundcloud.com/me/followings/" + userId,
              "", cb)
    }

    // ---- XHR in page context -------------------------------------------
    function write(method, url, body, cb) {
        var wv = ensureWebView()
        if (!wv || !wv.loaded) {
            pending.push({ m: method, u: url, b: body, cb: cb })
            return
        }
        runXhr(method, url, body, cb)
    }

    function flush() {
        var wv = webViewLoader.item
        if (!wv || !wv.loaded)
            return
        var items = pending
        pending = []
        for (var i = 0; i < items.length; i++)
            runXhr(items[i].m, items[i].u, items[i].b, items[i].cb)
    }

    function runXhr(method, url, body, cb) {
        var wv = webViewLoader.item
        if (!wv) {
            if (cb) cb(false, "no webview")
            return
        }
        // NOTE: runJavaScript requires a TOP-LEVEL return statement.
        var token = String(auth.accessToken || "")
        var js = ""
        js += "var x = new XMLHttpRequest();\n"
        js += "x.open('" + method + "', " + JSON.stringify(url) + ", true);\n"
        js += "x.withCredentials = true;\n"
        if (token.length > 0)
            js += "x.setRequestHeader('Authorization', 'OAuth ' + " + JSON.stringify(token) + ");\n"
        js += "x.setRequestHeader('Content-Type', 'application/json');\n"
        js += "x.onload = function() { window.__scWriteResult = x.status + '|' + x.responseText; };\n"
        js += "x.onerror = function() { window.__scWriteResult = '0|xhr-error'; };\n"
        js += "try { x.send(" + (body && body.length > 0 ? JSON.stringify(body) : "null") + "); }\n"
        js += "catch (e) { return '0|' + e; }\n"
        js += "return 'pending';\n"

        wv.runJavaScript(js, function() {
            pollResult(cb, 0)
        })
    }

    function pollResult(cb, attempt) {
        var wv = webViewLoader.item
        if (!wv) {
            if (cb) cb(false, "no webview")
            return
        }
        wv.runJavaScript("return window.__scWriteResult || 'pending';", function(result) {
            var s = String(result === undefined || result === null ? "" : result)
            if (s === "pending" || s.length === 0) {
                if (attempt < 25) {
                    pollTimer.pendingCb = cb
                    pollTimer.pendingAttempt = attempt + 1
                    pollTimer.restart()
                } else if (cb) {
                    cb(false, "timeout")
                }
                return
            }
            wv.runJavaScript("window.__scWriteResult = undefined; return 1;", function() {})
            var bar = s.indexOf("|")
            var status = parseInt(bar >= 0 ? s.substring(0, bar) : s, 10)
            var ok = (status === 200 || status === 201 || status === 204)
            if (cb)
                cb(ok, bar >= 0 ? s.substring(bar + 1, Math.min(s.length, 160)) : s)
        })
    }

    Timer {
        id: pollTimer
        property var pendingCb
        property int pendingAttempt: 0
        interval: 200
        onTriggered: pollResult(pendingCb, pendingAttempt)
    }
}
