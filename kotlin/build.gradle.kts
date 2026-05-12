import java.io.File

plugins {
    kotlin("jvm") version "1.9.24"
    `java-library`
    `maven-publish`
}

group = "io.tinywindow"
version = "0.1.0-SNAPSHOT"

repositories {
    mavenCentral()
}

dependencies {
    testImplementation(kotlin("test"))
    testImplementation("org.junit.jupiter:junit-jupiter:5.10.2")
    testRuntimeOnly("org.junit.platform:junit-platform-launcher")
}

tasks.test {
    useJUnitPlatform()
    jvmArgs("--add-opens", "java.base/java.lang=ALL-UNNAMED")
    systemProperty("java.library.path", layout.buildDirectory.dir("natives").get().asFile.absolutePath)
}

// --- Native library build via CMake ---

val nativeDir = layout.buildDirectory.dir("natives")
val coreDir = rootProject.layout.projectDirectory.dir("../core")

val cmakeConfigure by tasks.registering(Exec::class) {
    group = "native"
    description = "Configure CMake build for nanofilter core"
    workingDir = layout.buildDirectory.dir("cmake-build").get().asFile
    doFirst { workingDir.mkdirs() }
    commandLine("cmake",
        "-DCMAKE_BUILD_TYPE=Release",
        coreDir.asFile.absolutePath
    )
}

val cmakeBuild by tasks.registering(Exec::class) {
    group = "native"
    description = "Build nanofilter native library"
    dependsOn(cmakeConfigure)
    workingDir = layout.buildDirectory.dir("cmake-build").get().asFile
    commandLine("cmake", "--build", ".", "--parallel")
    doLast {
        // Copy built library to natives dir
        val nDir = nativeDir.get().asFile
        nDir.mkdirs()
        workingDir.listFiles()?.filter {
            it.name.endsWith(".so") || it.name.endsWith(".dylib") || it.name.endsWith(".dll")
        }?.forEach { it.copyTo(File(nDir, it.name), overwrite = true) }
    }
}

// --- JNI header generation ---

val jniDir = layout.projectDirectory.dir("src/main/c")

val compileJni by tasks.registering(Exec::class) {
    group = "native"
    description = "Compile JNI bridge"
    dependsOn(cmakeBuild)
    val javaHome = System.getProperty("java.home") ?: System.getenv("JAVA_HOME") ?: "/usr/lib/jvm/default"
    val outputLib = if (org.gradle.internal.os.OperatingSystem.current().isMacOsX) "libtinywindow_jni.dylib" else "libtinywindow_jni.so"
    val nDir = nativeDir.get().asFile
    doFirst { nDir.mkdirs() }

    val cmakeBuildDir = layout.buildDirectory.dir("cmake-build").get().asFile.absolutePath
    // Static-link libnanofilter.a so the JNI lib is self-contained (no runtime dependency)
    val osInclude = if (org.gradle.internal.os.OperatingSystem.current().isMacOsX) "darwin" else "linux"
    commandLine("cc", "-shared", "-fPIC", "-O2", "-pthread",
        "-I", "${coreDir.asFile.absolutePath}/include",
        "-I", "$javaHome/include",
        "-I", "$javaHome/include/$osInclude",
        jniDir.file("nanofilter_jni.c").asFile.absolutePath,
        "$cmakeBuildDir/libnanofilter.a",
        "-lm",
        "-o", "${nDir.absolutePath}/$outputLib"
    )
}

tasks.named("compileKotlin") {
    dependsOn(compileJni)
}

// Include native libs in JAR
tasks.named<Jar>("jar") {
    from(nativeDir) {
        into("native")
    }
}

kotlin {
    jvmToolchain(17)
}

publishing {
    publications {
        create<MavenPublication>("maven") {
            from(components["java"])
            pom {
                name.set("tinywindow")
                description.set("Time-windowed probabilistic frequency counting for JVM")
                url.set("https://github.com/Hail-cali/tinywindow")
                licenses {
                    license {
                        name.set("Apache License 2.0")
                        url.set("https://www.apache.org/licenses/LICENSE-2.0")
                    }
                }
            }
        }
    }
}
