build_lib(
  LIBNAME futurenet-qos

  SOURCE_FILES
    model/futurenet-traffic-tag.cc
    model/futurenet-deadline-tag.cc
    model/futurenet-queue-disc.cc
    model/futurenet-edf-queue.cc

  HEADER_FILES
    model/futurenet-traffic-tag.h
    model/futurenet-deadline-tag.h
    model/futurenet-queue-disc.h
    model/futurenet-edf-queue.h

  TEST_SOURCES
    test/futurenet-traffic-tag-test.cc
    test/futurenet-deadline-tag-test.cc
    test/futurenet-queue-disc-test.cc

  LIBRARIES_TO_LINK
    ${libcore}
    ${libnetwork}
)

