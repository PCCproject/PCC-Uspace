#include "channel_simulator.h"
#include "../src/core/options.h"

int main(int argc, char** argv) {
  Options::Parse(argc, argv);
  FILE* sender_config = fopen("sender.config", "r");
 
  double dur = 100;
  if (Options::Get("--sim-dur=") != NULL) {
    dur = atof(Options::Get("--sim-dur="));
  }
  
  double test_bw = 70;
  if (Options::Get("--test-bw=") != NULL) {
    test_bw = atof(Options::Get("--test-bw="));
  }
  
  Simulator simulator = Simulator(sender_config, test_bw, 0.03, 500, 0.0, dur);

  fclose(sender_config);

  LinkChangeEventData lc;
  lc.bw = 100;
  lc.bw_range = 90;
  lc.dl = 0.06;
  lc.dl_range = 0.05;
  lc.bf = 10000;
  lc.bf_range = 9000;
  lc.plr = 0.02;
  lc.plr_range = 0.02;
  lc.reset_queue = true;
  lc.change_interval = 30.0;

  EventInfo lc_event(LINK_CHANGE, 0.0, 0, 0, 0.0);
  lc_event.data = (void*)(&lc);

  if (Options::Get("--sim-variable-link") != NULL) {
    simulator.EnqueueEvent(lc_event);
  }

  simulator.Run();
  simulator.Analyze();

  return 0;
}
