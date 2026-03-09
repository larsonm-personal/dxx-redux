package com.dxxredux.app

/** Minimal JSON5 comment stripper. Removes // and /* */ comments so the
 *  result can be parsed by org.json. Does not handle comments inside strings. */
object Json5 {
    fun strip(text: String): String {
        val sb = StringBuilder(text.length)
        var i = 0
        while (i < text.length) {
            if (i + 1 < text.length && text[i] == '/' && text[i + 1] == '/') {
                // line comment — skip to end of line
                i += 2
                while (i < text.length && text[i] != '\n') i++
            } else if (i + 1 < text.length && text[i] == '/' && text[i + 1] == '*') {
                // block comment — skip to */
                i += 2
                while (i + 1 < text.length && !(text[i] == '*' && text[i + 1] == '/')) i++
                i += 2 // skip closing */
            } else {
                sb.append(text[i])
                i++
            }
        }
        return sb.toString()
    }
}
