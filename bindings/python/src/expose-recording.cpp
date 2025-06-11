#include "fwd.hpp"
#include "candlewick/utils/VideoRecorder.h"

using candlewick::media::VideoRecorder;

void exposeVideoRecorder() {
  using VideoRecorderSettings = VideoRecorder::Settings;
#define _c(name, doc) def_readwrite(#name, &VideoRecorderSettings::name, doc)

  bp::class_<VideoRecorderSettings>("VideoRecorderSettings", bp::no_init)
      .def(bp::init<>("self"_a))
      ._c(fps, "Frame rate.")
      ._c(bitRate, "Video bitrate.")
      ._c(outputWidth, "Output video width.")
      ._c(outputHeight, "Output video height.");
#undef _c
}
