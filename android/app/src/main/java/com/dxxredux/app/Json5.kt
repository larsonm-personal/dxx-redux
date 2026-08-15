package com.dxxredux.app

/** Minimal JSON5 comment stripper. Removes // and /* */ comments so the result can be parsed by org.json. */
object Json5 {
    fun strip(text: String): String {
        val sb = StringBuilder(text.length)
        var i = 0
        var inString = false
        var escaped = false
        while (i < text.length) {
            val character = text[i]
            if (inString) {
                sb.append(character)
                if (escaped) {
                    escaped = false
                } else if (character == '\\') {
                    escaped = true
                } else if (character == '"') {
                    inString = false
                }
                i++
            } else if (character == '"') {
                inString = true
                sb.append(character)
                i++
            } else if (i + 1 < text.length && character == '/' && text[i + 1] == '/') {
                // Line comment: skip to the end of the line
                i += 2
                while (i < text.length && text[i] != '\n') i++
            } else if (i + 1 < text.length && character == '/' && text[i + 1] == '*') {
                // Block comment: retain newlines for useful parser line numbers
                i += 2
                while (i < text.length) {
                    if (i + 1 < text.length && text[i] == '*' && text[i + 1] == '/') {
                        i += 2
                        break
                    }
                    if (text[i] == '\n') sb.append('\n')
                    i++
                }
            } else {
                sb.append(character)
                i++
            }
        }
        return sb.toString()
    }
}
