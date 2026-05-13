package io.tinywindow

import java.io.File
import java.nio.file.Files

internal object Native {

    init {
        loadNativeLibrary()
    }

    private fun loadNativeLibrary() {
        // 1st: try java.library.path (container direct placement via -Djava.library.path)
        try {
            System.loadLibrary("tinywindow_jni")
            return
        } catch (_: UnsatisfiedLinkError) { }

        // 2nd: extract platform-specific library from JAR
        val platform = detectPlatform()
        val resourcePath = "/native/${platform.classifier}/${platform.libName}"

        val resource = Native::class.java.getResourceAsStream(resourcePath)
            ?: throw UnsatisfiedLinkError(
                "Native library not found in JAR: $resourcePath " +
                "(os.name=${System.getProperty("os.name")}, os.arch=${System.getProperty("os.arch")})"
            )

        val tmpDir = Files.createTempDirectory("tinywindow").toFile()
        tmpDir.deleteOnExit()
        val tmpLib = File(tmpDir, platform.libName)
        tmpLib.deleteOnExit()

        resource.use { input ->
            tmpLib.outputStream().use { output ->
                input.copyTo(output)
            }
        }
        System.load(tmpLib.absolutePath)
    }

    private data class Platform(val classifier: String, val libName: String)

    private fun detectPlatform(): Platform {
        val osName = System.getProperty("os.name").lowercase()
        val osArch = System.getProperty("os.arch").lowercase()

        val os = when {
            "mac" in osName || "darwin" in osName -> "darwin"
            "win" in osName -> "windows"
            else -> "linux"
        }
        val arch = when (osArch) {
            "amd64", "x86_64" -> "amd64"
            "aarch64", "arm64" -> "aarch64"
            else -> osArch
        }
        val libName = when (os) {
            "darwin" -> "libtinywindow_jni.dylib"
            "windows" -> "tinywindow_jni.dll"
            else -> "libtinywindow_jni.so"
        }
        return Platform(classifier = "$os-$arch", libName = libName)
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
