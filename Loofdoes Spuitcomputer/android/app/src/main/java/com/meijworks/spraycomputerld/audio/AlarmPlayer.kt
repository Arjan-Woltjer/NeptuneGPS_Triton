package com.meijworks.spraycomputerld.audio

import android.content.Context
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioTrack
import android.media.Ringtone
import android.media.RingtoneManager
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.util.Log

/**
 * Plays the selected alarm. Synthesised sounds go through a static AudioTrack
 * on the ALARM stream so they are heard even when the phone is on silent or
 * the screen is off. The "phone alarm" option plays the system alarm tone.
 */
class AlarmPlayer(private val context: Context) {
    private val handler = Handler(Looper.getMainLooper())
    private val pcmCache = HashMap<AlarmSound, ShortArray>()

    private var track: AudioTrack? = null
    private var trackSound: AlarmSound? = null
    private var ringtone: Ringtone? = null
    private var stopRunnable: Runnable? = null

    @Volatile var volume: Float = 1f
        set(value) {
            field = value.coerceIn(0f, 1f)
            track?.setVolume(field)
        }

    private val attributes = AudioAttributes.Builder()
        .setUsage(AudioAttributes.USAGE_ALARM)
        .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
        .build()

    /** Pre-render and pre-load a sound so the first touch plays instantly. */
    fun prepare(sound: AlarmSound) {
        if (!sound.isSynthesized) {
            releaseTrack()
            return
        }
        if (trackSound == sound && track != null) return
        releaseTrack()
        val pcm = pcmCache.getOrPut(sound) { sound.render() }
        try {
            val t = AudioTrack.Builder()
                .setAudioAttributes(attributes)
                .setAudioFormat(
                    AudioFormat.Builder()
                        .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                        .setSampleRate(SAMPLE_RATE)
                        .setChannelMask(AudioFormat.CHANNEL_OUT_MONO)
                        .build()
                )
                .setTransferMode(AudioTrack.MODE_STATIC)
                .setBufferSizeInBytes(pcm.size * 2)
                .build()
            t.write(pcm, 0, pcm.size)
            t.setVolume(volume)
            track = t
            trackSound = sound
        } catch (e: Exception) {
            Log.e(TAG, "Cannot create AudioTrack", e)
        }
    }

    /**
     * Start the alarm. With [loop] the sound repeats until [stop] is called,
     * otherwise it plays once (or, for the system tone, for about a second).
     */
    fun play(sound: AlarmSound, loop: Boolean) {
        cancelScheduledStop()
        if (sound.isSynthesized) {
            stopRingtone()
            prepare(sound)
            val t = track ?: return
            val frames = pcmCache[sound]?.size ?: return
            try {
                t.stop()
                t.reloadStaticData()
                t.setLoopPoints(0, frames, if (loop) -1 else 0)
                t.setVolume(volume)
                t.play()
            } catch (e: IllegalStateException) {
                Log.w(TAG, "AudioTrack in bad state, recreating", e)
                releaseTrack()
                prepare(sound)
                try {
                    track?.setLoopPoints(0, frames, if (loop) -1 else 0)
                    track?.play()
                } catch (e2: Exception) {
                    Log.e(TAG, "Playback failed", e2)
                }
            }
        } else {
            track?.let { runCatching { it.stop() } }
            playRingtone(loop)
        }
    }

    /** Stop whatever is playing. */
    fun stop() {
        cancelScheduledStop()
        track?.let { runCatching { it.stop() } }
        stopRingtone()
    }

    fun release() {
        stop()
        releaseTrack()
    }

    private fun playRingtone(loop: Boolean) {
        stopRingtone()
        val uri = RingtoneManager.getDefaultUri(RingtoneManager.TYPE_ALARM)
            ?: RingtoneManager.getDefaultUri(RingtoneManager.TYPE_NOTIFICATION)
            ?: return
        val r = RingtoneManager.getRingtone(context, uri) ?: return
        r.audioAttributes = attributes
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            r.isLooping = loop
            r.volume = volume
        }
        ringtone = r
        r.play()
        if (!loop) {
            val stopper = Runnable { stopRingtone() }
            stopRunnable = stopper
            handler.postDelayed(stopper, ONE_SHOT_RINGTONE_MS)
        }
    }

    private fun stopRingtone() {
        ringtone?.let { runCatching { if (it.isPlaying) it.stop() } }
        ringtone = null
    }

    private fun cancelScheduledStop() {
        stopRunnable?.let { handler.removeCallbacks(it) }
        stopRunnable = null
    }

    private fun releaseTrack() {
        track?.let {
            runCatching { it.stop() }
            runCatching { it.release() }
        }
        track = null
        trackSound = null
    }

    companion object {
        private const val TAG = "AlarmPlayer"
        private const val ONE_SHOT_RINGTONE_MS = 1500L
    }
}
