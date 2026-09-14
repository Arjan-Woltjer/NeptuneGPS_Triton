# Release builds run R8 with resource shrinking (NeptuneGPS_Triton#63).
# The app has no reflection of its own: the BLE client, the protocol parser
# and the service are all called directly, and Compose, coroutines and the
# AndroidX libraries ship their own consumer rules. Keep this file for the
# day something reflected (a serialiser, a native library) is added.

# Readable stack traces from operators' bug reports: keep line numbers and
# hide the original file names behind one tag.
-keepattributes SourceFile,LineNumberTable
-renamesourcefileattribute SourceFile
