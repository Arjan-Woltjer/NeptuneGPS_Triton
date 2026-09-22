/*
  SprayComputer LD - Android companion app for the haulm sprayer computer
  Copyright (C) 2026 J.A. Woltjer.

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

package nl.meijworks.spraycomputerld

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.w3c.dom.Element
import java.io.File
import javax.xml.parsers.DocumentBuilderFactory

/**
 * Guards the four `strings.xml` files (NeptuneGPS_Triton#142).
 *
 * These read the resource XML off disk rather than through `R`, because what
 * is being checked is the *files*: that they agree with each other, and that
 * the text in them survives contact with `getString`. Neither is visible from
 * a compiled resource id.
 *
 * Both rules come from real bugs. The `%%` one shipped in all four languages
 * and was found by looking at a tablet, not by any tool in the build.
 */
class StringResourcesTest {

    private data class Entry(val name: String, val text: String, val translatable: Boolean)

    private val locales = listOf("nl", "de", "fr")

    @Test
    fun everyLocaleDefinesExactlyTheTranslatableKeys() {
        val expected = read(null).filter { it.translatable }.map { it.name }.toSortedSet()
        assertTrue("no strings found; is the res path right?", expected.size > 100)

        for (locale in locales) {
            val actual = read(locale).map { it.name }.toSortedSet()
            assertEquals(
                "values-$locale is missing translations",
                emptySet<String>(),
                (expected - actual).toSet(),
            )
            // An extra key is either a typo or a translation of something the
            // default file marks translatable="false", which aapt keeps but
            // nothing will ever read.
            assertEquals(
                "values-$locale defines strings the default file does not",
                emptySet<String>(),
                (actual - expected).toSet(),
            )
        }
    }

    @Test
    fun doubledPercentIsOnlyUsedWhereArgumentsAreFormatted() {
        // Android collapses %% to % only when the string is resolved WITH
        // format arguments. In a string that takes none, getString hands the
        // doubled sign straight to the screen, which is what "5 %%" did.
        val positional = Regex("""%\d+\$""")
        val offenders = mutableListOf<String>()

        for (locale in listOf(null) + locales) {
            for (entry in read(locale)) {
                if ("%%" in entry.text && !positional.containsMatchIn(entry.text)) {
                    offenders += "values${locale?.let { "-$it" } ?: ""}/${entry.name}"
                }
            }
        }

        assertEquals(
            "these use %% but take no format arguments, so the second % reaches the screen; " +
                "write a single % and mark the string formatted=\"false\"",
            emptyList<String>(),
            offenders,
        )
    }

    // ------------------------------------------------------------- reading

    private fun read(locale: String?): List<Entry> {
        val dir = if (locale == null) "values" else "values-$locale"
        val file = File(resDir(), "$dir/strings.xml")
        assertTrue("missing ${file.path}", file.isFile)

        val doc = DocumentBuilderFactory.newInstance().newDocumentBuilder().parse(file)
        val nodes = doc.getElementsByTagName("string")
        return (0 until nodes.length).map { i ->
            val el = nodes.item(i) as Element
            Entry(
                name = el.getAttribute("name"),
                text = el.textContent,
                translatable = el.getAttribute("translatable") != "false",
            )
        }
    }

    /**
     * Gradle runs unit tests with the module directory as the working
     * directory, but that is a default rather than a promise, so walk up
     * until the resource tree appears instead of trusting it.
     */
    private fun resDir(): File {
        var dir: File? = File("").absoluteFile
        while (dir != null) {
            val candidate = File(dir, "src/main/res")
            if (candidate.isDirectory) return candidate
            val nested = File(dir, "app/src/main/res")
            if (nested.isDirectory) return nested
            dir = dir.parentFile
        }
        throw AssertionError("could not find src/main/res from ${File("").absolutePath}")
    }
}
