# Stage 1: Build C core + JNI native library (linux-amd64)
FROM eclipse-temurin:17-jdk AS native-builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake build-essential && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY core/ core/
COPY kotlin/src/main/c/ kotlin/src/main/c/

RUN cd core && mkdir -p build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    make -j$(nproc)

RUN mkdir -p /build/natives/linux-amd64 && \
    cc -shared -fPIC -O2 -pthread \
      -I /build/core/include \
      -I $JAVA_HOME/include \
      -I $JAVA_HOME/include/linux \
      /build/kotlin/src/main/c/tinywindow_jni.c \
      /build/core/build/libtinywindow.a \
      -lm \
      -o /build/natives/linux-amd64/libtinywindow_jni.so

# Stage 2: Build Kotlin JAR
FROM eclipse-temurin:17-jdk AS jar-builder

WORKDIR /app
COPY --from=native-builder /build/natives/ kotlin/build/natives/
COPY . .

RUN cd kotlin && ./gradlew jar -x cmakeConfigure -x cmakeBuild -x compileJni --no-daemon

# Stage 3: Runtime (glibc-based distroless)
FROM gcr.io/distroless/java17-debian13:nonroot

COPY --from=native-builder /build/natives/linux-amd64/libtinywindow_jni.so /app/lib/
COPY --from=jar-builder /app/kotlin/build/libs/*.jar /app/app.jar

# Load native library directly from /app/lib — no /tmp extraction needed
# Compatible with readOnlyRootFilesystem: true
ENTRYPOINT ["java", "-Djava.library.path=/app/lib", "-jar", "/app/app.jar"]
