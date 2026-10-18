// Simple formatting helpers (no singleton)

function formatCount(n) {
    n = Number(n) || 0
    if (n >= 1000000)
        return (n / 1000000).toFixed(1) + "M"
    if (n >= 1000)
        return (n / 1000).toFixed(1) + "k"
    return String(n)
}

function formatDuration(ms) {
    ms = Number(ms) || 0
    var s = Math.floor(ms / 1000)
    var m = Math.floor(s / 60)
    s = s % 60
    return m + ":" + (s < 10 ? "0" : "") + s
}
