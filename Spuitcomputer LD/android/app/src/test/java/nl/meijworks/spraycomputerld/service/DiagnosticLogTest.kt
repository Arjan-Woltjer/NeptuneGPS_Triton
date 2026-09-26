package nl.meijworks.spraycomputerld.service

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.time.ZoneId

/** The ring buffer behind the log export (NeptuneGPS_Triton#128). */
class DiagnosticLogTest {

    private val utc = ZoneId.of("UTC")

    /** 2026-09-27 20:47:25.678 UTC. */
    private val fixed = 1790542_045_678L

    private fun logAt(millis: Long, maxBytes: Int = DiagnosticLog.MAX_BYTES) =
        DiagnosticLog(maxBytes = maxBytes, clock = { millis }, zone = utc)

    @Test
    fun `stamps every line to the millisecond`() {
        val log = logAt(fixed)
        log.append("> PING")

        // Milliseconds, not seconds: status lines are 200 ms apart and the
        // order within one second is what a dropout investigation turns on.
        assertEquals(listOf("20:47:25.678 > PING"), log.snapshot())
    }

    @Test
    fun `keeps lines in the order they arrived`() {
        val log = logAt(fixed)
        log.append("> CAL GET")
        log.append("< C:D,0,0,1")
        log.append("< OK")

        assertEquals(
            listOf("> CAL GET", "< C:D,0,0,1", "< OK"),
            log.snapshot().map { it.substringAfter(' ') },
        )
    }

    @Test
    fun `evicts the oldest lines once the byte bound is passed`() {
        // Small bound so the test states the arithmetic rather than looping
        // two megabytes: each line below weighs 12 (stamp) + 1 (space) +
        // 4 (body) + 1 (newline) = 18 bytes.
        val log = logAt(fixed, maxBytes = 40)
        log.append("aaaa")
        log.append("bbbb")
        assertEquals(36, log.byteCount())

        log.append("cccc")

        assertEquals(36, log.byteCount())
        assertEquals(listOf("bbbb", "cccc"), log.snapshot().map { it.substringAfter(' ') })
    }

    @Test
    fun `bounds by bytes not lines, so long lines cost more`() {
        val log = logAt(fixed, maxBytes = 60)
        log.append("x".repeat(40))       // one fat line, 54 bytes
        log.append("aaaa")               // 19 more would pass 60

        // The fat line went, not some fixed number of short ones. A line-count
        // bound would have kept both and held far less time with NMEA on.
        assertEquals(listOf("aaaa"), log.snapshot().map { it.substringAfter(' ') })
    }

    @Test
    fun `a single line larger than the whole buffer is still kept`() {
        val log = logAt(fixed, maxBytes = 10)
        log.append("x".repeat(100))

        // Evicting to empty would throw away the only evidence there is.
        assertEquals(1, log.snapshot().size)
        assertTrue(log.byteCount() > 10)
    }

    @Test
    fun `counts UTF-8 bytes, not characters`() {
        val log = logAt(fixed)
        log.append("e")
        val ascii = log.byteCount()

        log.clear()
        log.append("é")             // e-acute: two bytes in UTF-8

        assertEquals(ascii + 1, log.byteCount())
    }

    @Test
    fun `clear empties the buffer and its byte count`() {
        val log = logAt(fixed)
        log.append("> PING")
        log.clear()

        assertEquals(emptyList<String>(), log.snapshot())
        assertEquals(0, log.byteCount())
    }

    @Test
    fun `render puts the header above a separator and ends with a newline`() {
        val log = logAt(fixed)
        log.append("> PING")

        val text = log.render(listOf("SprayComputer LD 1.0.0", "Firmware 0.2, protocol 2"))

        assertEquals(
            "SprayComputer LD 1.0.0\n" +
                "Firmware 0.2, protocol 2\n" +
                "--\n" +
                "20:47:25.678 > PING\n",
            text,
        )
    }

    @Test
    fun `the default bound is the two megabytes the issue asked for`() {
        assertEquals(2 * 1024 * 1024, DiagnosticLog.MAX_BYTES)
    }
}
