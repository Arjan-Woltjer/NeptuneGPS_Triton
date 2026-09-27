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

package nl.meijworks.spraycomputerld.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Rect
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.StrokeJoin
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.TextMeasurer
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.unit.dp
import kotlin.math.ceil
import kotlin.math.floor
import kotlin.math.log10
import kotlin.math.max
import kotlin.math.pow

/** One calibration point: a board reading or duty on x, what it means on y. */
data class GraphPoint(val x: Int, val y: Int)

/**
 * A calibration table drawn as the board uses it: its points, joined by
 * straight lines, because the board interpolates linearly between them
 * (NeptuneGPS_Triton#179). Pompkalibratie draws flow against PWM duty,
 * Potmeterkalibratie dose against the knob reading, where the bend at the
 * middle point shows a logarithmic potentiometer.
 *
 * One series, so no legend: the card title names it. The three states are
 * never colour alone — each has its own shape and a text label:
 * the point being redone is a larger marker, its old position a hollow ring
 * labelled [wasLabel], and a refused measurement an ✕ labelled
 * [refusedLabel]. The values themselves are in the list under the graph.
 */
@Composable
fun CalibrationGraph(
    points: List<GraphPoint>,
    xLabel: String,
    yLabel: String,
    modifier: Modifier = Modifier,
    xMax: Int = 4095,
    highlight: Int? = null,
    highlightLabel: String? = null,
    was: GraphPoint? = null,
    wasLabel: String? = null,
    refused: GraphPoint? = null,
    refusedLabel: String? = null,
) {
    val colors = MaterialTheme.colorScheme
    val series = colors.primary
    val surface = colors.surfaceContainerHighest   // the Card's own surface: markers ring in it
    val grid = colors.outlineVariant
    val muted = colors.onSurfaceVariant
    val ink = colors.onSurface
    val error = colors.error
    val tickStyle = MaterialTheme.typography.labelSmall.copy(color = muted)
    val noteStyle = MaterialTheme.typography.labelSmall.copy(color = ink)
    val measurer = rememberTextMeasurer()

    Canvas(modifier.fillMaxWidth().height(196.dp)) {
        val allY = points.map { it.y } + listOfNotNull(was?.y, refused?.y)
        val yStep = niceStep((allY.maxOrNull() ?: 1).coerceAtLeast(1) / 4.0)
        val yTop = max(yStep, ceil((allY.maxOrNull() ?: 1) / yStep) * yStep)
        val yTicks = generateSequence(0.0) { it + yStep }.takeWhile { it <= yTop + 1e-9 }.toList()
        val xTicks = (0..xMax step 1000).toList()

        // Gutters sized from the widest tick label, so nothing is clipped.
        val yLabelWidth = yTicks.maxOf { measurer.measure(tick(it), tickStyle).size.width }
        val left = yLabelWidth + 6.dp.toPx()
        val top = measurer.measure(yLabel, tickStyle).size.height + 6.dp.toPx()
        // Two lines below the plot: the x ticks, then the x title on its own,
        // or the title lands on top of the last tick.
        val tickHeight = measurer.measure("0", tickStyle).size.height
        val xTitleHeight = measurer.measure(xLabel, tickStyle).size.height
        val bottom = size.height - tickHeight - xTitleHeight - 6.dp.toPx()
        val right = size.width - 8.dp.toPx()

        fun px(p: GraphPoint) = Offset(
            left + (right - left) * p.x.coerceIn(0, xMax) / xMax.toFloat(),
            bottom - (bottom - top) * (p.y / yTop).toFloat(),
        )

        // Recessive hairline grid, one per y tick; the zero line is the x axis.
        val hair = 1.dp.toPx()
        yTicks.forEach { v ->
            val y = bottom - (bottom - top) * (v / yTop).toFloat()
            drawLine(grid, Offset(left, y), Offset(right, y), strokeWidth = hair)
            val t = measurer.measure(tick(v), tickStyle)
            drawText(t, topLeft = Offset(left - 6.dp.toPx() - t.size.width, y - t.size.height / 2f))
        }
        xTicks.forEach { v ->
            val x = left + (right - left) * v / xMax.toFloat()
            val t = measurer.measure(v.toString(), tickStyle)
            drawText(t, topLeft = Offset((x - t.size.width / 2f).coerceIn(0f, size.width - t.size.width), bottom + 2.dp.toPx()))
        }
        drawText(measurer.measure(yLabel, tickStyle), topLeft = Offset(0f, 0f))
        val xl = measurer.measure(xLabel, tickStyle)
        drawText(xl, topLeft = Offset(right - xl.size.width, size.height - xl.size.height))

        if (points.isEmpty()) return@Canvas

        // The curve: 2 dp, round joins and caps.
        val path = Path().apply {
            points.forEachIndexed { i, p -> px(p).let { if (i == 0) moveTo(it.x, it.y) else lineTo(it.x, it.y) } }
        }
        drawPath(path, series, style = Stroke(width = 2.dp.toPx(), cap = StrokeCap.Round, join = StrokeJoin.Round))

        // Markers with a 2 dp surface ring; the point being redone is larger.
        points.forEachIndexed { i, p ->
            val c = px(p)
            val r = if (i == highlight) 7.dp.toPx() else 4.dp.toPx()
            drawCircle(surface, radius = r + 2.dp.toPx(), center = c)
            drawCircle(series, radius = r, center = c)
        }

        // The old position of a redone point: a hollow ring, drawn over the
        // markers with its own surface halo, so a new point close by cannot
        // hide it.
        was?.let {
            val c = px(it)
            drawCircle(surface, radius = 5.dp.toPx(), center = c, style = Stroke(width = 5.dp.toPx()))
            drawCircle(muted, radius = 5.dp.toPx(), center = c, style = Stroke(width = 2.dp.toPx()))
        }

        // A refused measurement: an ✕ where it would have landed.
        refused?.let {
            val c = px(it)
            val a = 5.dp.toPx()
            val w = 2.dp.toPx()
            drawLine(error, Offset(c.x - a, c.y - a), Offset(c.x + a, c.y + a), strokeWidth = w, cap = StrokeCap.Round)
            drawLine(error, Offset(c.x - a, c.y + a), Offset(c.x + a, c.y - a), strokeWidth = w, cap = StrokeCap.Round)
        }

        // Labels last, measured together.
        val notes = buildList {
            val hp = highlight?.let { points.getOrNull(it) }
            if (hp != null && highlightLabel != null) add(Note(highlightLabel, noteStyle, px(hp), 11.dp.toPx()))
            if (was != null && wasLabel != null) add(Note(wasLabel, tickStyle, px(was), 9.dp.toPx()))
            if (refused != null && refusedLabel != null) add(Note(refusedLabel, noteStyle, px(refused), 9.dp.toPx()))
        }
        placeNotes(measurer, notes)
    }
}

private class Note(val text: String, val style: TextStyle, val at: Offset, val gap: Float)

/**
 * Each label centred above its mark, or below it when there is no room
 * above. When two labels would overlap — a redone point landing close to its
 * old position does exactly that — the one belonging to the lower mark moves
 * below it, so neither is stacked on the other.
 */
private fun DrawScope.placeNotes(measurer: TextMeasurer, notes: List<Note>) {
    val layouts = notes.map { measurer.measure(it.text, it.style) }
    fun rect(i: Int, above: Boolean): Rect {
        val n = notes[i]
        val t = layouts[i]
        val x = (n.at.x - t.size.width / 2f).coerceIn(0f, size.width - t.size.width)
        val y = if (above) n.at.y - n.gap - t.size.height else n.at.y + n.gap
        return Rect(Offset(x, y), Size(t.size.width.toFloat(), t.size.height.toFloat()))
    }
    val rects = notes.indices.map { i -> rect(i, true).let { if (it.top >= 0f) it else rect(i, false) } }.toMutableList()
    for (i in notes.indices) for (j in i + 1 until notes.size) {
        if (rects[i].overlaps(rects[j])) {
            val lower = if (notes[i].at.y >= notes[j].at.y) i else j
            rects[lower] = rect(lower, false)
        }
    }
    notes.indices.forEach { i -> drawText(layouts[i], topLeft = rects[i].topLeft) }
}

/** 1, 2, 2.5 or 5 times a power of ten: ticks a reader can add up. */
private fun niceStep(raw: Double): Double {
    if (raw <= 0.0) return 1.0
    val magnitude = 10.0.pow(floor(log10(raw)))
    val f = raw / magnitude
    val nice = when {
        f <= 1.0 -> 1.0
        f <= 2.0 -> 2.0
        f <= 2.5 -> 2.5
        f <= 5.0 -> 5.0
        else -> 10.0
    }
    return max(1.0, nice * magnitude)
}

private fun tick(v: Double): String =
    if (v == floor(v)) v.toLong().toString() else "%.1f".format(v)
