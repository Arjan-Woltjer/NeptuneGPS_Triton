package nl.meijworks.spraycomputerld

import android.content.Context
import androidx.annotation.StringRes
import androidx.compose.runtime.Composable
import androidx.compose.ui.platform.LocalContext

/**
 * Text produced away from a composition — in the service and the calibration
 * wizard — that still has to appear in the operator's language.
 *
 * Those layers cannot call `stringResource`, and resolving with a `Context`
 * at the point of production would freeze the text in whatever locale was
 * current then. Carrying the resource id instead means the string is
 * resolved when it is drawn, and the UI can ask *which* message this is
 * (`(msg as? UiText.Res)?.id == R.string.msg_board_refused`) rather than
 * matching on English text, which no longer holds once translated.
 */
sealed interface UiText {

    /** A string resource, with the arguments it formats. */
    data class Res(@StringRes val id: Int, val args: List<Any> = emptyList()) : UiText

    /** Text that comes from the board and is not ours to translate. */
    data class Raw(val text: String) : UiText

    fun resolve(context: Context): String = when (this) {
        is Res -> if (args.isEmpty()) context.getString(id) else context.getString(id, *args.toTypedArray())
        is Raw -> text
    }
}

/** `uiText(R.string.msg_board_refused, reason)` reads better than the constructor. */
fun uiText(@StringRes id: Int, vararg args: Any): UiText.Res = UiText.Res(id, args.toList())

@Composable
fun UiText.text(): String = resolve(LocalContext.current)

@Composable
fun UiText?.textOrEmpty(): String = this?.resolve(LocalContext.current) ?: ""

/** True when this is exactly [id], whatever language it renders in. */
fun UiText?.isRes(@StringRes id: Int): Boolean = (this as? UiText.Res)?.id == id
