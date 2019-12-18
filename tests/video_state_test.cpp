#include <player/media/video_state.h>

int main(int argc, char** argv) {
  UNUSED(argc);
  UNUSED(argv);
  using namespace fastoplayer;
  media::VideoState* vs =
      new media::VideoState("id", common::uri::Url(), media::AppOptions(), media::ComplexOptions());
  delete vs;
  return 0;
}
