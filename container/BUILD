load("@bazel_tools//tools/build_defs/pkg:pkg.bzl", "pkg_tar")
load("@io_bazel_rules_docker//container:container.bzl", "container_image", "container_push")

container_image(
    name = "dlp_proxy",
    # This depends on istio_proxy container_pull in WORKSPACE
    base = "@istio_proxy//image",
    files = ["//plugin:filter.wasm"],
    symlinks = {
        "/etc/istio/extensions/dlp-filter.wasm": "/filter.wasm",
    },
    tars = [":licenses_tar"],
)

pkg_tar(
    name = "licenses_tar",
    srcs = ["//third_party/container:licenses"],
    mode = "0755",
    package_dir = "/",
    strip_prefix = "/third_party/container",
)

container_push(
    name = "dlp_proxy_push",
    format = "Docker",
    image = ":dlp_proxy",
    registry = "$(CONTAINER_REGISTRY)",
    repository = "$(CONTAINER_REPOSITORY)",
    tag = "$(CONTAINER_TAG)",
)
