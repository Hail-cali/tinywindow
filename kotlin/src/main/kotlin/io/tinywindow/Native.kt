package io.tinywindow

import java.io.File
import java.nio.file.Files

internal object Native {

    init {
        loadNativeLibrary()
    }

    private fun loadNativeLibrary() {
        try {
            System.loadLibrary("tinywindow_jni")
            return
        } catch (_: UnsatisfiedLinkError) { }

        val osName = System.getProperty("os.name").lowercase()
        val libName = when {
            "mac" in osName || "darwin" in osName -> "libtinywindow_jni.dylib"
            "win" in osName -> "tinywindow_jni.dll"
            else -> "libtinywindow_jni.so"
        }

        val resource = Native::class.java.getResourceAsStream("/native/$libName")
            ?: throw UnsatisfiedLinkError("Native library not found in JAR: /native/$libName")

        val tmpDir = Files.createTempDirectory("tinywindow").toFile()
        tmpDir.deleteOnExit()
        val tmpLib = File(tmpDir, libName)
        tmpLib.deleteOnExit()

        resource.use { input ->
            tmpLib.outputStream().use { output ->
                input.copyTo(output)
            }
        }
        System.load(tmpLib.absolutePath)
    }

    // Timing CMS
    @JvmStatic external fun tcmsCreate(depth: Int, width: Int, numSlots: Int, slotDurationMs: Long): Long
    @JvmStatic external fun tcmsDestroy(handle: Long)
    @JvmStatic external fun tcmsRecord(handle: Long, key: String, nowMs: Long): Int
    @JvmStatic external fun tcmsCount(handle: Long, key: String, windowMs: Long, nowMs: Long): Long
    @JvmStatic external fun tcmsReset(handle: Long)
    @JvmStatic external fun tcmsMemoryUsage(handle: Long): Long

    // Sliding Bloom Filter
    @JvmStatic external fun sbfCreate(expectedItems: Long, fpRate: Double, numSlots: Int, slotDurationMs: Long): Long
    @JvmStatic external fun sbfDestroy(handle: Long)
    @JvmStatic external fun sbfInsert(handle: Long, key: String, nowMs: Long): Int
    @JvmStatic external fun sbfMightContain(handle: Long, key: String, windowMs: Long, nowMs: Long): Boolean
    @JvmStatic external fun sbfMemoryUsage(handle: Long): Long
}
