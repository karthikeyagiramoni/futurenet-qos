
build_lib_example(
  NAME futurenet-qos-example
  SOURCE_FILES futurenet-qos-example.cc
  LIBRARIES_TO_LINK
    ${libfuturenet-qos}
    ${libcore}
    ${libnetwork}
    ${libtraffic-control}
)
