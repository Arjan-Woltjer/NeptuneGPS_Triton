package nl.meijworks.spraycomputerld

import android.app.Application
import android.app.NotificationChannel
import android.app.NotificationManager
import nl.meijworks.spraycomputerld.service.SprayerService

class SprayComputerApp : Application() {
    override fun onCreate() {
        super.onCreate()
        Settings.init(this)
        val manager = getSystemService(NotificationManager::class.java)
        val channel = NotificationChannel(
            SprayerService.CHANNEL_ID,
            getString(R.string.notification_channel_name),
            NotificationManager.IMPORTANCE_LOW
        ).apply {
            description = getString(R.string.notification_channel_description)
            setShowBadge(false)
        }
        manager.createNotificationChannel(channel)
    }
}
