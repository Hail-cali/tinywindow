# K8s Deployment Example

Multi-stage Dockerfile for deploying a Spring Boot service with tinywindow on Kubernetes.

## Build & Run

```bash
# Copy Dockerfile and .dockerignore to project root, then:
docker build -t my-app .
docker run --rm my-app
```

## K8s Pod (readOnlyRootFilesystem)

tinywindow loads the native library via `-Djava.library.path=/app/lib`,
so no `/tmp` extraction is needed. `readOnlyRootFilesystem: true` works out of the box.

If your Spring Boot app needs a writable `/tmp` for Tomcat, add an emptyDir volume:

```yaml
spec:
  containers:
    - name: app
      securityContext:
        readOnlyRootFilesystem: true
        runAsNonRoot: true
      env:
        - name: SERVER_TOMCAT_BASEDIR
          value: "/tmp/tomcat"
      volumeMounts:
        - name: tmp
          mountPath: /tmp
      resources:
        limits:
          # JVM heap + tinywindow off-heap (~17MB for 5M pairs x 3 windows)
          memory: "512Mi"
  volumes:
    - name: tmp
      emptyDir:
        sizeLimit: 64Mi
```
