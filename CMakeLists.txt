build_lib(
  LIBNAME futurenet-qos

  SOURCE_FILES
    model/futurenet-traffic-tag.cc

  HEADER_FILES
    model/futurenet-traffic-tag.h

  LIBRARIES_TO_LINK
    ${libcore}
    ${libnetwork}
)
