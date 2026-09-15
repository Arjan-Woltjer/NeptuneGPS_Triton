package nl.meijworks.spraycomputerld.ble

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.BluetoothLeScanner
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.os.ParcelUuid
import android.util.Log
import androidx.core.content.ContextCompat
import nl.meijworks.spraycomputerld.ConnectionState
import java.util.ArrayDeque
import java.util.UUID

/**
 * BLE central for the Loofdoes board: scans for it, connects, negotiates a
 * larger MTU, subscribes to the event characteristic and reconnects whenever
 * the link drops. Notifications are reassembled into lines on the newline
 * (the board splits long lines over several notifications); commands go out
 * one at a time through a queue, each terminated with a newline.
 * All callbacks to [Listener] are delivered on the main thread.
 *
 * Derived from the Buzzer-game client, with the board's own service UUIDs
 * so neither app ever connects to the other's device.
 */
@SuppressLint("MissingPermission")
class SprayerBleClient(private val context: Context, private val listener: Listener) {

    interface Listener {
        fun onConnectionState(state: ConnectionState, deviceName: String?)
        fun onLine(line: String)
        fun onError(message: String)
        /** True while Android asks the user for the board's pairing code. */
        fun onPairing(active: Boolean)
    }

    private class Outgoing(val bytes: ByteArray, val secure: Boolean)

    private val main = Handler(Looper.getMainLooper())
    private val adapter: BluetoothAdapter? =
        (context.getSystemService(Context.BLUETOOTH_SERVICE) as? BluetoothManager)?.adapter

    @Volatile private var enabled = false
    private var scanner: BluetoothLeScanner? = null
    private var scanning = false
    private var gatt: BluetoothGatt? = null
    private var controlChar: BluetoothGattCharacteristic? = null
    private var secureChar: BluetoothGattCharacteristic? = null
    private var pairing = false
    private var inFlight: Outgoing? = null
    private var state = ConnectionState.OFF

    private val rxBuffer = StringBuilder()
    private val txQueue = ArrayDeque<Outgoing>()
    private var txInFlight = false

    private val scanTimeout = Runnable { restartScan("scan timeout") }
    private val connectTimeout = Runnable { dropConnection("connect timeout") }
    private val reconnect = Runnable { if (enabled) startScan() }
    private val txTimeout = Runnable {
        if (pairing) return@Runnable
        Log.w(TAG, "write callback never came, continuing")
        txInFlight = false
        inFlight = null
        drainTx()
    }

    val isConnected: Boolean get() = state == ConnectionState.CONNECTED

    // Bond state, to resend the command that triggered pairing once it is done.
    private val bondReceiver = object : BroadcastReceiver() {
        override fun onReceive(ctx: Context, intent: Intent) {
            if (intent.action != BluetoothDevice.ACTION_BOND_STATE_CHANGED) return
            val state = intent.getIntExtra(BluetoothDevice.EXTRA_BOND_STATE, BluetoothDevice.BOND_NONE)
            main.post {
                if (!pairing) return@post
                when (state) {
                    BluetoothDevice.BOND_BONDED -> {
                        Log.i(TAG, "bonded")
                        pairing = false
                        listener.onPairing(false)
                        // The stack resends the pending write itself; if its
                        // callback never comes, the timeout resends ours.
                        if (txInFlight) main.postDelayed(txTimeout, TX_TIMEOUT_MS * 5) else drainTx()
                    }
                    BluetoothDevice.BOND_NONE -> {
                        Log.w(TAG, "pairing cancelled or failed")
                        pairing = false
                        inFlight = null
                        txQueue.clear()
                        listener.onPairing(false)
                        listener.onError("Pairing cancelled; the command was not sent")
                    }
                }
            }
        }
    }
    private var bondReceiverRegistered = false

    fun start() {
        if (enabled) return
        enabled = true
        if (!bondReceiverRegistered) {
            ContextCompat.registerReceiver(
                context, bondReceiver, IntentFilter(BluetoothDevice.ACTION_BOND_STATE_CHANGED),
                ContextCompat.RECEIVER_EXPORTED,
            )
            bondReceiverRegistered = true
        }
        startScan()
    }

    fun stop() {
        enabled = false
        if (bondReceiverRegistered) {
            runCatching { context.unregisterReceiver(bondReceiver) }
            bondReceiverRegistered = false
        }
        main.removeCallbacks(reconnect)
        stopScan()
        closeGatt()
        setState(ConnectionState.OFF, null)
    }

    /**
     * Queue one command line for the board; the newline is added here.
     * [secure] sends it on the bonded characteristic, which makes Android
     * pair with the board the first time (the board shows its code).
     */
    fun sendCommand(command: String, secure: Boolean = false): Boolean {
        if (gatt == null || controlChar == null) return false
        if (secure && secureChar == null) return false
        txQueue.add(Outgoing((command + "\n").toByteArray(Charsets.US_ASCII), secure))
        drainTx()
        return true
    }

    private fun drainTx() {
        if (txInFlight || pairing) return
        val g = gatt ?: run { txQueue.clear(); return }
        val next = inFlight ?: txQueue.poll() ?: return
        inFlight = next
        val c = (if (next.secure) secureChar else controlChar) ?: run { inFlight = null; txQueue.clear(); return }
        // First protected command to a board this phone is not bonded with:
        // the write request goes out, the board answers "insufficient
        // authentication", and Android's stack starts LE pairing on its own,
        // posting a pairing notification and resending the write once the
        // bond exists. createBond() is deliberately not used: on the bench it
        // chose classic Bluetooth PIN pairing, which the board cannot do.
        if (next.secure && g.device.bondState != BluetoothDevice.BOND_BONDED && !pairing) {
            pairing = true
            listener.onPairing(true)
            Log.i(TAG, "protected write to an unbonded board: expecting the pairing prompt")
        }
        val bytes = next.bytes
        // Protected commands must be write *requests*: a write without response
        // to an encrypted characteristic is silently discarded by the peripheral,
        // so Android never gets the "insufficient authentication" error that
        // makes it pair. The open channel keeps the cheaper no-response write.
        val type = if (next.secure) BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
                   else BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
        val ok = try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                g.writeCharacteristic(c, bytes, type) ==
                    BluetoothGatt.GATT_SUCCESS
            } else {
                @Suppress("DEPRECATION")
                run {
                    c.writeType = type
                    c.value = bytes
                    g.writeCharacteristic(c)
                }
            }
        } catch (e: Exception) {
            Log.w(TAG, "write failed", e)
            false
        }
        if (ok) {
            txInFlight = true
            // A protected write can sit in the stack for as long as the
            // operator takes to enter the code; the timeout is checked for
            // that in txTimeout.
            main.postDelayed(txTimeout, TX_TIMEOUT_MS)
        } else {
            listener.onError("Write failed")
            main.postDelayed({ drainTx() }, 50)
        }
    }

    // ---------------------------------------------------------------- scanning

    private fun startScan() {
        main.removeCallbacks(reconnect)
        val a = adapter
        if (a == null || !a.isEnabled) {
            listener.onError("Bluetooth is off")
            setState(ConnectionState.OFF, null)
            main.postDelayed(reconnect, RECONNECT_DELAY_MS * 3)
            return
        }
        if (!hasPermissions(context)) {
            listener.onError("Bluetooth permission missing")
            setState(ConnectionState.OFF, null)
            return
        }
        if (scanning || gatt != null) return
        val s = a.bluetoothLeScanner ?: run {
            listener.onError("BLE scanner unavailable")
            main.postDelayed(reconnect, RECONNECT_DELAY_MS * 3)
            return
        }
        scanner = s
        val filters = listOf(
            ScanFilter.Builder().setServiceUuid(ParcelUuid(SERVICE_UUID)).build(),
            ScanFilter.Builder().setDeviceName(DEVICE_NAME).build(),
        )
        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()
        try {
            s.startScan(filters, settings, scanCallback)
            scanning = true
            setState(ConnectionState.SCANNING, null)
            main.postDelayed(scanTimeout, SCAN_PERIOD_MS)
            Log.i(TAG, "scan started")
        } catch (e: Exception) {
            Log.e(TAG, "startScan failed", e)
            listener.onError("Scan failed: ${e.message}")
            main.postDelayed(reconnect, RECONNECT_DELAY_MS * 3)
        }
    }

    private fun stopScan() {
        main.removeCallbacks(scanTimeout)
        if (!scanning) return
        scanning = false
        try {
            scanner?.stopScan(scanCallback)
        } catch (e: Exception) {
            Log.w(TAG, "stopScan failed", e)
        }
    }

    private fun restartScan(reason: String) {
        Log.i(TAG, "restart scan: $reason")
        stopScan()
        if (enabled) main.postDelayed(reconnect, RECONNECT_DELAY_MS)
    }

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            main.post { onDeviceFound(result.device) }
        }

        override fun onBatchScanResults(results: MutableList<ScanResult>) {
            results.firstOrNull()?.let { r -> main.post { onDeviceFound(r.device) } }
        }

        override fun onScanFailed(errorCode: Int) {
            main.post {
                scanning = false
                listener.onError("Scan failed (code $errorCode)")
                if (enabled) main.postDelayed(reconnect, RECONNECT_DELAY_MS * 3)
            }
        }
    }

    // -------------------------------------------------------------- connecting

    private fun onDeviceFound(device: BluetoothDevice) {
        if (!enabled || gatt != null) return
        stopScan()
        Log.i(TAG, "connecting to ${device.address}")
        setState(ConnectionState.CONNECTING, safeName(device))
        gatt = device.connectGatt(context, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
        main.postDelayed(connectTimeout, CONNECT_TIMEOUT_MS)
    }

    private fun dropConnection(reason: String) {
        Log.i(TAG, "dropping connection: $reason")
        closeGatt()
        if (enabled) {
            setState(ConnectionState.SCANNING, null)
            main.postDelayed(reconnect, RECONNECT_DELAY_MS)
        } else {
            setState(ConnectionState.OFF, null)
        }
    }

    private fun closeGatt() {
        main.removeCallbacks(connectTimeout)
        main.removeCallbacks(txTimeout)
        controlChar = null
        secureChar = null
        txQueue.clear()
        inFlight = null
        txInFlight = false
        if (pairing) { pairing = false; listener.onPairing(false) }
        rxBuffer.setLength(0)
        gatt?.let {
            runCatching { it.disconnect() }
            runCatching { it.close() }
        }
        gatt = null
    }

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
            main.post {
                if (g !== gatt) { runCatching { g.close() }; return@post }
                if (newState == BluetoothProfile.STATE_CONNECTED && status == BluetoothGatt.GATT_SUCCESS) {
                    Log.i(TAG, "connected, requesting MTU")
                    g.requestConnectionPriority(BluetoothGatt.CONNECTION_PRIORITY_HIGH)
                    // A status line is ~50 bytes; at the default MTU it needs three
                    // notifications. Ask for more first, discover services after.
                    if (!g.requestMtu(PREFERRED_MTU)) {
                        if (!g.discoverServices()) dropConnection("discoverServices refused")
                    }
                } else {
                    dropConnection("state=$newState status=$status")
                }
            }
        }

        override fun onMtuChanged(g: BluetoothGatt, mtu: Int, status: Int) {
            main.post {
                if (g !== gatt) return@post
                Log.i(TAG, "MTU $mtu (status $status)")
                if (!g.discoverServices()) dropConnection("discoverServices refused")
            }
        }

        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
            main.post {
                if (g !== gatt) return@post
                val service = g.getService(SERVICE_UUID)
                val event = service?.getCharacteristic(EVENT_CHAR_UUID)
                val control = service?.getCharacteristic(CONTROL_CHAR_UUID)
                secureChar = service?.getCharacteristic(SECURE_CHAR_UUID)   // absent on a pre-#53 board
                if (status != BluetoothGatt.GATT_SUCCESS || service == null || event == null || control == null) {
                    dropConnection("service not found (status $status)")
                    return@post
                }
                controlChar = control
                if (!g.setCharacteristicNotification(event, true)) {
                    dropConnection("cannot enable notifications")
                    return@post
                }
                val cccd = event.getDescriptor(CCCD_UUID)
                if (cccd == null) {
                    dropConnection("CCCD missing")
                    return@post
                }
                val ok = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                    g.writeDescriptor(cccd, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE) ==
                        BluetoothGatt.GATT_SUCCESS
                } else {
                    @Suppress("DEPRECATION")
                    run {
                        cccd.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                        g.writeDescriptor(cccd)
                    }
                }
                if (!ok) dropConnection("CCCD write refused")
            }
        }

        override fun onDescriptorWrite(g: BluetoothGatt, descriptor: BluetoothGattDescriptor, status: Int) {
            main.post {
                if (g !== gatt) return@post
                if (status == BluetoothGatt.GATT_SUCCESS) {
                    main.removeCallbacks(connectTimeout)
                    setState(ConnectionState.CONNECTED, safeName(g.device))
                } else {
                    dropConnection("CCCD write failed ($status)")
                }
            }
        }

        override fun onCharacteristicWrite(g: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) {
            main.post {
                if (g !== gatt) return@post
                main.removeCallbacks(txTimeout)
                txInFlight = false
                if (status == GATT_INSUFFICIENT_AUTHENTICATION || status == GATT_INSUFFICIENT_ENCRYPTION) {
                    // The stack starts bonding on its own; the command stays in
                    // flight and goes out again once the bond is there.
                    Log.i(TAG, "write needs pairing (status $status)")
                    if (!pairing) { pairing = true; listener.onPairing(true) }
                    return@post
                }
                if (status != BluetoothGatt.GATT_SUCCESS) Log.w(TAG, "write status $status")
                inFlight = null
                drainTx()
            }
        }

        // Android 13+
        override fun onCharacteristicChanged(
            g: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            value: ByteArray,
        ) {
            deliver(characteristic.uuid, value)
        }

        // Android 12 and lower
        @Deprecated("Deprecated in API 33, still needed for older devices")
        @Suppress("DEPRECATION")
        override fun onCharacteristicChanged(g: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
                deliver(characteristic.uuid, characteristic.value ?: return)
            }
        }

        private fun deliver(uuid: UUID, value: ByteArray) {
            if (uuid != EVENT_CHAR_UUID) return
            val text = String(value, Charsets.US_ASCII)
            main.post {
                rxBuffer.append(text)
                while (true) {
                    val nl = rxBuffer.indexOf("\n")
                    if (nl < 0) break
                    val line = rxBuffer.substring(0, nl).trimEnd('\r')
                    rxBuffer.delete(0, nl + 1)
                    if (line.isNotEmpty()) listener.onLine(line)
                }
                if (rxBuffer.length > MAX_PENDING_CHARS) rxBuffer.setLength(0)
            }
        }
    }

    private fun safeName(device: BluetoothDevice?): String? =
        try { device?.name ?: DEVICE_NAME } catch (e: SecurityException) { DEVICE_NAME }

    private fun setState(newState: ConnectionState, name: String?) {
        if (state == newState && newState != ConnectionState.CONNECTED) return
        state = newState
        listener.onConnectionState(newState, name)
    }

    companion object {
        private const val TAG = "SprayerBle"
        // Must equal BleSprayer::kDeviceName in the firmware.
        const val DEVICE_NAME = "SprayComputer LD"
        val SERVICE_UUID: UUID = UUID.fromString("7c1a0001-4b6e-4c0f-9c3a-2f1d5e8a0001")
        val CONTROL_CHAR_UUID: UUID = UUID.fromString("7c1a0002-4b6e-4c0f-9c3a-2f1d5e8a0001")
        val EVENT_CHAR_UUID: UUID = UUID.fromString("7c1a0003-4b6e-4c0f-9c3a-2f1d5e8a0001")
        val SECURE_CHAR_UUID: UUID = UUID.fromString("7c1a0004-4b6e-4c0f-9c3a-2f1d5e8a0001")
        private const val GATT_INSUFFICIENT_AUTHENTICATION = 5
        private const val GATT_INSUFFICIENT_ENCRYPTION = 15
        val CCCD_UUID: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")

        private const val PREFERRED_MTU = 247
        private const val SCAN_PERIOD_MS = 20_000L
        private const val CONNECT_TIMEOUT_MS = 12_000L
        private const val RECONNECT_DELAY_MS = 1_500L
        private const val TX_TIMEOUT_MS = 1_000L
        private const val MAX_PENDING_CHARS = 512

        fun requiredPermissions(): Array<String> =
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
            } else {
                arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)
            }

        fun hasPermissions(context: Context): Boolean = requiredPermissions().all {
            ContextCompat.checkSelfPermission(context, it) == PackageManager.PERMISSION_GRANTED
        }
    }
}
