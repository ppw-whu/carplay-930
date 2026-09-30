// Host tests: compile this file with a C++11 compiler, outside the Arduino sketch.
#include "../MotionControl.h"
#include <stdio.h>
#include <stdlib.h>
static int checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
static void state(MotionControl& c, uint32_t t, ControlSource source, int l, int r) {
  const MotionState s = c.update(t);
  CHECK(s.source == source); CHECK(s.left == l); CHECK(s.right == r);
}
static void activate(MotionControl& c, uint32_t t) {
  c.receivePS3(0, 0, false, false, t);
  c.receivePS3(0, 0, true, false, t + 1);
  CHECK(c.ps3ModeActive());
  state(c, t + 1, ControlSource::PS3, 0, 0);
}
static void mixer() {
  MotionControl c(500,100);
  const int cases[][4] = {{100,0,100,100},{-100,0,-100,-100},{0,100,100,-100},
    {0,-100,-100,100},{200,100,255,85},{200,-100,85,255},{1000,0,255,255}};
  for (unsigned i=0; i<sizeof(cases)/sizeof(cases[0]); ++i) {
    c.submit(ControlSource::WiFi,cases[i][0],cases[i][1],0,500);
    state(c,0,ControlSource::WiFi,cases[i][2],cases[i][3]);
  }
  for(int speed=-255;speed<=255;++speed) for(int turn=-255;turn<=255;++turn) {
    c.submit(ControlSource::WiFi,speed,turn,0,500);
    MotionState a=c.update(0);
    CHECK(a.left>=-255 && a.left<=255); CHECK(a.right>=-255 && a.right<=255);
    c.submit(ControlSource::WiFi,speed,-turn,0,500);
    MotionState b=c.update(0); CHECK(a.left==b.right); CHECK(a.right==b.left);
    c.submit(ControlSource::WiFi,-speed,-turn,0,500);
    b=c.update(0); CHECK(a.left==-b.left); CHECK(a.right==-b.right);
  }
}
static void activationAndLatch() {
  MotionControl c(500,100);
  c.receivePS3(100,0,false,false,0);
  state(c,0,ControlSource::None,0,0); // joystick alone cannot activate
  CHECK(c.submit(ControlSource::PS3,100,0,0,500)==SubmitResult::RemoteInactive);
  c.receivePS3(100,0,true,false,1); // reject activation while stick deflected
  CHECK(!c.ps3ModeActive());
  c.receivePS3(0,0,true,false,2); // holding combo then centering does not activate
  CHECK(!c.ps3ModeActive());
  activate(c,3);
  c.receivePS3(100,0,false,false,5); // no shoulder buttons needed after activation
  state(c,5,ControlSource::PS3,100,100);
  c.receivePS3(-100,0,false,false,6);
  state(c,6,ControlSource::PS3,-100,-100);
  c.receivePS3(0,0,false,false,7);
  state(c,7,ControlSource::PS3,0,0); CHECK(c.ps3ModeActive());
  c.receivePS3(0,0,true,false,8); CHECK(c.ps3ModeActive()); // combo is not a toggle
}
static void rotationPriority() {
  MotionControl c(500,100); activate(c,0);
  c.receivePS3(0,-160,false,false,2); state(c,2,ControlSource::PS3,-160,160);
  c.receivePS3(0,160,false,false,3); state(c,3,ControlSource::PS3,160,-160);
  c.receivePS3(160,80,false,false,4); state(c,4,ControlSource::PS3,80,-80);
  c.receivePS3(-160,-80,false,false,5); state(c,5,ControlSource::PS3,-80,80);
  c.receivePS3(160,0,false,false,6); state(c,6,ControlSource::PS3,160,160);
}
static void ownership() {
  MotionControl c(500,100);
  c.submit(ControlSource::WiFi,100,0,0,1000);
  c.receivePS3(100,0,false,false,1); state(c,1,ControlSource::WiFi,100,100);
  activate(c,2); // activation clears old WiFi and stops immediately at neutral
  CHECK(c.submit(ControlSource::WiFi,80,0,4,500)==SubmitResult::PS3Busy);
  c.receivePS3(0,0,false,false,5); // even centered active remote retains ownership
  CHECK(c.submit(ControlSource::WiFi,80,0,5,500)==SubmitResult::PS3Busy);
  c.receivePS3(0,0,false,true,6); // Cross exits
  state(c,6,ControlSource::None,0,0); CHECK(!c.ps3ModeActive());
  state(c,100,ControlSource::None,0,0); // no old WiFi resume
  CHECK(c.submit(ControlSource::WiFi,80,0,101,500)==SubmitResult::Accepted);
  c.receivePS3(100,0,false,false,102); state(c,102,ControlSource::WiFi,80,80);
}
static void stopAndFreshCombo() {
  MotionControl c(500,100); activate(c,0);
  c.receivePS3(100,0,false,false,2);
  c.receivePS3(0,0,true,true,3); // Cross wins even if activation combo is held
  state(c,3,ControlSource::None,0,0); CHECK(!c.ps3ModeActive()); CHECK(c.ps3Online(3));
  c.receivePS3(0,0,true,false,4); CHECK(!c.ps3ModeActive());
  activate(c,5);
  c.stop(); state(c,7,ControlSource::None,0,0); CHECK(!c.ps3ModeActive());
  c.receivePS3(0,0,true,false,8); CHECK(!c.ps3ModeActive());
  activate(c,9);
  c.stop(ControlSource::PS3,100); CHECK(c.ps3Online(599));
  state(c,600,ControlSource::None,0,0); CHECK(!c.ps3Online(600));
}
static void timeoutAndDisconnect() {
  MotionControl c(500,100); activate(c,0);
  c.receivePS3(120,0,false,false,2);
  state(c,501,ControlSource::PS3,120,120);
  state(c,502,ControlSource::None,0,0); CHECK(!c.ps3ModeActive());
  c.receivePS3(0,0,true,false,503); CHECK(!c.ps3ModeActive());
  c.receivePS3(120,0,false,false,504); state(c,504,ControlSource::None,0,0);
  activate(c,505);
  c.receivePS3(120,0,false,false,1005); state(c,1504,ControlSource::PS3,120,120);
  c.receivePS3(0,0,true,false,1505); // submit itself notices timeout before new packet
  CHECK(!c.ps3ModeActive()); state(c,1505,ControlSource::None,0,0);
  activate(c,1506); c.disconnectWiFi(); CHECK(c.ps3ModeActive());
  c.disconnectPS3(); CHECK(!c.ps3ModeActive()); CHECK(!c.ps3Online(1508));
  c.receivePS3(0,0,true,false,1509); CHECK(!c.ps3ModeActive());
  activate(c,1510);
}
static void wifiTimeout() {
  MotionControl c(500,100);
  c.submit(ControlSource::WiFi,100,0,10,500); state(c,509,ControlSource::WiFi,100,100);
  state(c,510,ControlSource::None,0,0);
  c.submit(ControlSource::WiFi,100,0,600,500);
  c.submit(ControlSource::WiFi,-80,0,700,100); state(c,799,ControlSource::WiFi,-80,-80);
  state(c,800,ControlSource::None,0,0);
  c.submit(ControlSource::WiFi,100,0,900,0); state(c,900,ControlSource::None,0,0);
  c.submit(ControlSource::WiFi,100,0,901,500); c.disconnectWiFi();
  state(c,902,ControlSource::None,0,0);
}
static void rollover() {
  const uint32_t start=UINT32_MAX-49u;
  MotionControl c(100,100); activate(c,start-2u);
  c.receivePS3(100,0,false,false,start);
  state(c,49,ControlSource::PS3,100,100);
  state(c,50,ControlSource::None,0,0); CHECK(!c.ps3ModeActive());
  c.receivePS3(0,0,true,false,51); CHECK(!c.ps3ModeActive());
  state(c,151,ControlSource::None,0,0);
  state(c,51,ControlSource::None,0,0); CHECK(!c.ps3Online(51)); // full counter cycle
  MotionControl wifi;
  wifi.submit(ControlSource::WiFi,70,0,start,100);
  state(wifi,49,ControlSource::WiFi,70,70); state(wifi,50,ControlSource::None,0,0);
}
static void auxiliaryPS3() {
  MotionControl c(500,100);
  CHECK(c.update(0).auxLevel == 0);
  c.receivePS3(0,0,false,false,0,true); // inactive triangle cannot start motor
  CHECK(c.update(0).auxLevel == 0);
  c.receivePS3(0,0,true,false,1,true); // held during activation: release required
  CHECK(c.ps3ModeActive()); CHECK(c.update(1).auxLevel == 0);
  c.receivePS3(0,0,false,false,2,true);
  CHECK(c.update(2).auxLevel == 0);
  uint32_t t = 3;
  for (int expected = 1; expected <= 4; ++expected) {
    c.receivePS3(0,0,false,false,t++,false);
    c.receivePS3(0,0,false,false,t++,true);
    CHECK(c.update(t-1).auxLevel == expected % 4);
    for (int i = 0; i < 20; ++i) {
      c.receivePS3(0,0,false,false,t++,true);
      CHECK(c.update(t-1).auxLevel == expected % 4);
    }
  }
  c.receivePS3(100,0,false,false,t++,false);
  c.receivePS3(100,0,false,false,t++,true);
  CHECK(c.update(t-1).auxLevel == 1);
  state(c,t-1,ControlSource::PS3,100,100);
  c.receivePS3(0,0,false,false,t++,false); // centered wheels leave aux running
  CHECK(c.update(t-1).auxLevel == 1);
  c.disconnectWiFi(); CHECK(c.update(t-1).auxLevel == 1); // late browser release
  c.receivePS3(0,0,false,true,t++,true); // Cross wins over triangle
  CHECK(c.update(t-1).auxLevel == 0); CHECK(!c.ps3ModeActive());
  activate(c,t); t += 2;
  c.receivePS3(0,0,false,false,t++,true);
  CHECK(c.update(t-1).auxLevel == 1);
  CHECK(c.update(t+499).auxLevel == 0); CHECK(!c.ps3ModeActive());
  t += 500; activate(c,t); t += 2;
  c.receivePS3(0,0,false,false,t,true);
  CHECK(c.update(t).auxLevel == 1);
  c.disconnectPS3(); CHECK(c.update(t).auxLevel == 0);
}

static void auxiliaryWiFi() {
  MotionControl c(500,100);
  for (int level = 0; level <= 3; ++level) {
    CHECK(c.submit(ControlSource::WiFi,0,0,level*100,500,level)==SubmitResult::Accepted);
    for (int i = 0; i < 20; ++i) {
      c.submit(ControlSource::WiFi,0,0,level*100+i,500,level);
      CHECK(c.update(level*100+i).auxLevel == level); // retries are idempotent
    }
  }
  c.submit(ControlSource::WiFi,160,0,400,500); // backwards compatible omission
  state(c,400,ControlSource::WiFi,160,160); CHECK(c.update(400).auxLevel == 3);
  c.receivePS3(0,0,false,false,401); // unarmed PS3/disconnect must not stop WiFi aux
  c.disconnectPS3(); CHECK(c.update(401).auxLevel == 3);
  CHECK(c.update(899).auxLevel == 3); CHECK(c.update(900).auxLevel == 0);
  c.submit(ControlSource::WiFi,0,0,901,500); CHECK(c.update(901).auxLevel == 0);
  c.submit(ControlSource::WiFi,0,0,902,500,2);
  activate(c,903); CHECK(c.update(904).auxLevel == 0); // takeover starts off
  c.receivePS3(0,0,false,false,905,true); CHECK(c.update(905).auxLevel == 1);
  CHECK(c.submit(ControlSource::WiFi,0,0,906,500,3)==SubmitResult::PS3Busy);
  CHECK(c.update(906).auxLevel == 1);
  c.stop(); CHECK(c.update(907).auxLevel == 0);
  c.submit(ControlSource::WiFi,0,0,908,500,2);
  c.disconnectWiFi(); CHECK(c.update(909).auxLevel == 0);
  c.submit(ControlSource::WiFi,0,0,910,0,2); CHECK(c.update(910).auxLevel == 0);
  const uint32_t start = UINT32_MAX - 49u;
  MotionControl rolloverAux;
  rolloverAux.submit(ControlSource::WiFi,0,0,start,100,3);
  CHECK(rolloverAux.update(49).auxLevel == 3);
  CHECK(rolloverAux.update(50).auxLevel == 0);
}

static void ps3SpeedLevels() {
  MotionControl c;
  CHECK(c.ps3SpeedPercent() == 80);
  c.receivePS3(0,0,false,false,0,false,true); // square does nothing before activation
  CHECK(c.ps3SpeedPercent() == 80);
  c.receivePS3(0,0,true,false,1,false,true); // already held at activation
  CHECK(c.ps3ModeActive()); CHECK(c.ps3SpeedPercent() == 80);
  c.receivePS3(255,0,false,false,2,false,true);
  state(c,2,ControlSource::PS3,204,204);
  c.receivePS3(-255,0,false,false,3,false,true);
  state(c,3,ControlSource::PS3,-204,-204);
  c.receivePS3(128,0,false,false,4,false,true);
  state(c,4,ControlSource::PS3,102,102);
  c.receivePS3(-128,0,false,false,5,false,true);
  state(c,5,ControlSource::PS3,-102,-102);
  c.receivePS3(255,-255,false,false,6,false,true);
  state(c,6,ControlSource::PS3,-204,204); // rotation retains priority at reduced speed
  c.receivePS3(-255,255,false,false,7,false,true);
  state(c,7,ControlSource::PS3,204,-204);
  c.receivePS3(1000,0,false,false,8,false,true);
  state(c,8,ControlSource::PS3,204,204); // normalize/clamp before percentage scaling
  CHECK(c.ps3SpeedPercent() == 80);

  uint32_t t = 9;
  const int expectedPercent[] = {100,20,40,60,80,100,20};
  const int expectedPWM[] = {255,51,102,153,204,255,51};
  for (unsigned i=0; i<sizeof(expectedPercent)/sizeof(expectedPercent[0]); ++i) {
    c.receivePS3(255,0,false,false,t++,false,false);
    c.receivePS3(255,0,false,false,t++,false,true);
    CHECK(c.ps3SpeedPercent() == expectedPercent[i]);
    state(c,t-1,ControlSource::PS3,expectedPWM[i],expectedPWM[i]);
    for (int j=0; j<20; ++j) {
      c.receivePS3(255,0,false,false,t++,false,true);
      CHECK(c.ps3SpeedPercent() == expectedPercent[i]); // held square cannot repeat
    }
  }
  c.receivePS3(0,0,false,false,t++,false,false);
  c.receivePS3(0,0,false,false,t++,true,false);
  CHECK(c.update(t-1).auxLevel == 1); CHECK(c.ps3SpeedPercent() == 20);
  c.receivePS3(0,0,false,false,t++,false,false);
  c.receivePS3(255,0,false,false,t++,false,true);
  state(c,t-1,ControlSource::PS3,102,102);
  CHECK(c.update(t-1).auxLevel == 1); // speed and triangle retain independent state
  c.receivePS3(0,0,false,false,t++,false,false);
  c.receivePS3(0,0,false,false,t++,true,true);
  CHECK(c.update(t-1).auxLevel == 2); CHECK(c.ps3SpeedPercent() == 60);
  c.receivePS3(0,0,false,false,t++,false,false);
  c.receivePS3(255,0,true,true,t++,true,true); // Cross beats both toggles and activation
  CHECK(c.update(t-1).auxLevel == 0); CHECK(c.ps3SpeedPercent() == 80);
  CHECK(!c.ps3ModeActive()); state(c,t-1,ControlSource::None,0,0);
  activate(c,t); t += 2;
  c.receivePS3(255,0,false,false,t,false,false);
  state(c,t,ControlSource::PS3,204,204);
}

static void ps3SpeedResetAndRawAPI() {
  MotionControl c;
  activate(c,0);
  c.receivePS3(255,0,false,false,2,false,true);
  CHECK(c.ps3SpeedPercent() == 100);
  c.stop(); CHECK(c.ps3SpeedPercent() == 80);
  activate(c,3);
  c.receivePS3(255,0,false,false,5,false,true);
  CHECK(c.ps3SpeedPercent() == 100);
  c.disconnectPS3(); CHECK(c.ps3SpeedPercent() == 80);
  c.receivePS3(255,0,true,false,6,false,true);
  CHECK(!c.ps3ModeActive()); CHECK(c.ps3SpeedPercent() == 80);
  activate(c,7);
  c.receivePS3(255,0,false,false,9,false,true);
  CHECK(c.ps3SpeedPercent() == 100);
  state(c,508,ControlSource::PS3,255,255);
  state(c,509,ControlSource::None,0,0); CHECK(c.ps3SpeedPercent() == 80);
  activate(c,510);
  c.receivePS3(255,0,false,false,512,false,true);
  CHECK(c.ps3SpeedPercent() == 100);
  c.receivePS3(255,0,true,false,1012,false,true); // timeout detected in packet handler
  CHECK(!c.ps3ModeActive()); CHECK(c.ps3SpeedPercent() == 80);

  // Scaling is specific to normalized joystick input; the shared raw API stays PWM.
  CHECK(c.submit(ControlSource::WiFi,255,0,1013,500)==SubmitResult::Accepted);
  state(c,1013,ControlSource::WiFi,255,255);
  activate(c,1014);
  CHECK(c.submit(ControlSource::PS3,255,0,1016,500)==SubmitResult::Accepted);
  state(c,1016,ControlSource::PS3,255,255);
  c.receivePS3(255,0,false,false,1017);
  state(c,1017,ControlSource::PS3,204,204);

  MotionControl configured(500,60);
  CHECK(configured.ps3SpeedPercent() == 60);
  activate(configured,0);
  configured.receivePS3(255,0,false,false,2,false,true);
  CHECK(configured.ps3SpeedPercent() == 80);
  configured.stop(); CHECK(configured.ps3SpeedPercent() == 60);
}

static void wifiSessions() {
  MotionControl c;
  CHECK(c.wifiSession() == 0);
  const uint32_t first = c.claimWiFi(0);
  CHECK(first != 0); CHECK(c.wifiSession() == first);
  state(c,0,ControlSource::WiFi,0,0); CHECK(c.update(0).auxLevel == 0);
  CHECK(c.submit(ControlSource::WiFi,255,0,1,500,3)==SubmitResult::StaleSession);
  state(c,1,ControlSource::WiFi,0,0); CHECK(c.update(1).auxLevel == 0);
  CHECK(c.submit(ControlSource::WiFi,100,0,2,500,3,first)==SubmitResult::Accepted);
  state(c,2,ControlSource::WiFi,100,100); CHECK(c.update(2).auxLevel == 3);

  const uint32_t second = c.claimWiFi(3);
  CHECK(second != 0 && second != first); CHECK(c.wifiSession() == second);
  state(c,3,ControlSource::WiFi,0,0); CHECK(c.update(3).auxLevel == 0);
  CHECK(c.submit(ControlSource::WiFi,255,0,4,500,3,first)==SubmitResult::StaleSession);
  CHECK(!c.releaseWiFi(first)); CHECK(!c.releaseWiFi());
  state(c,4,ControlSource::WiFi,0,0); CHECK(c.wifiSession() == second);
  CHECK(c.submit(ControlSource::WiFi,0,-102,5,500,2,second)==SubmitResult::Accepted);
  state(c,5,ControlSource::WiFi,-102,102); CHECK(c.update(5).auxLevel == 2);
  CHECK(c.releaseWiFi(second)); CHECK(c.wifiSession() == 0);
  state(c,6,ControlSource::None,0,0); CHECK(c.update(6).auxLevel == 0);
  CHECK(c.submit(ControlSource::WiFi,255,0,7,500,3,second)==SubmitResult::StaleSession);
  // A legacy API client remains usable only outside a managed browser session.
  CHECK(c.submit(ControlSource::WiFi,51,0,8,500,1)==SubmitResult::Accepted);
  state(c,8,ControlSource::WiFi,51,51);
  CHECK(c.releaseWiFi()); state(c,9,ControlSource::None,0,0);

  const uint32_t third = c.claimWiFi(10);
  CHECK(third != first && third != second && third != 0);
  c.stop(); CHECK(c.wifiSession() == 0);
  CHECK(c.submit(ControlSource::WiFi,255,0,11,500,3,third)==SubmitResult::StaleSession);
  const uint32_t fourth = c.claimWiFi(12);
  c.disconnectWiFi(); CHECK(c.wifiSession() == 0);
  CHECK(!c.releaseWiFi(fourth));
  CHECK(c.submit(ControlSource::WiFi,255,0,13,500,3,fourth)==SubmitResult::StaleSession);

  const uint32_t fifth = c.claimWiFi(14);
  state(c,513,ControlSource::WiFi,0,0);
  state(c,514,ControlSource::None,0,0); CHECK(c.wifiSession() == 0);
  CHECK(c.submit(ControlSource::WiFi,255,0,515,500,3,fifth)==SubmitResult::StaleSession);
  const uint32_t sixth = c.claimWiFi(516);
  // A late heartbeat must detect its expired lease even before motorTask update.
  CHECK(c.submit(ControlSource::WiFi,255,0,1016,500,3,sixth)==SubmitResult::StaleSession);
  state(c,1016,ControlSource::None,0,0); CHECK(c.wifiSession() == 0);

  MotionControl wrapped;
  const uint32_t start = UINT32_MAX - 49u;
  const uint32_t rolloverSession = wrapped.claimWiFi(start,100);
  state(wrapped,49,ControlSource::WiFi,0,0);
  state(wrapped,50,ControlSource::None,0,0); CHECK(wrapped.wifiSession() == 0);
  CHECK(wrapped.submit(ControlSource::WiFi,100,0,51,100,1,rolloverSession)==SubmitResult::StaleSession);
}

static void explicitHandover() {
  MotionControl c;
  const uint32_t first = c.claimWiFi(0);
  c.submit(ControlSource::WiFi,100,0,1,500,3,first);
  activate(c,2); // Remote explicitly takes over; old browser cannot revive.
  CHECK(c.wifiSession() == 0); CHECK(c.update(3).auxLevel == 0);
  c.receivePS3(255,0,false,false,4,true,true);
  CHECK(c.ps3SpeedPercent() == 100); CHECK(c.update(4).auxLevel == 1);
  const uint32_t second = c.claimWiFi(5); // Browser explicitly takes over.
  CHECK(second != first && second != 0);
  CHECK(!c.ps3ModeActive()); CHECK(c.ps3SpeedPercent() == 80);
  state(c,5,ControlSource::WiFi,0,0); CHECK(c.update(5).auxLevel == 0);
  c.receivePS3(255,0,false,false,6,true,true); // Connected sticks/buttons do not reclaim.
  state(c,6,ControlSource::WiFi,0,0); CHECK(c.update(6).auxLevel == 0);
  CHECK(c.submit(ControlSource::WiFi,120,0,7,500,2,second)==SubmitResult::Accepted);
  CHECK(c.submit(ControlSource::WiFi,255,0,8,500,3,first)==SubmitResult::StaleSession);
  CHECK(!c.releaseWiFi(first));
  c.receivePS3(255,0,true,false,9); // Fresh combo while deflected is rejected.
  state(c,9,ControlSource::WiFi,120,120); CHECK(c.update(9).auxLevel == 2);
  c.receivePS3(0,0,true,false,10); // Centering with combo still held is not a new press.
  CHECK(!c.ps3ModeActive()); CHECK(c.wifiSession() == second);
  activate(c,11);
  CHECK(c.wifiSession() == 0); CHECK(c.update(12).auxLevel == 0);
  CHECK(c.submit(ControlSource::WiFi,255,0,13,500,3,second)==SubmitResult::PS3Busy);
  CHECK(c.submit(ControlSource::WiFi,255,0,13,500,3)==SubmitResult::PS3Busy);
  CHECK(!c.releaseWiFi(second)); // Delayed browser exit cannot cancel remote.
  CHECK(c.releaseWiFi()); CHECK(c.ps3ModeActive());
  c.receivePS3(-255,0,false,false,14);
  state(c,14,ControlSource::PS3,-204,-204);
  c.receivePS3(0,0,false,true,15); // Fresh Cross globally stops.
  state(c,15,ControlSource::None,0,0);

  const uint32_t third = c.claimWiFi(16);
  c.submit(ControlSource::WiFi,102,0,17,500,2,third);
  for (uint32_t t=18; t<23; ++t) {
    c.receivePS3(255,255,true,true,t,true,true); // A held Cross cannot repeatedly stop new owner.
    state(c,t,ControlSource::WiFi,102,102);
    CHECK(c.wifiSession() == third); CHECK(c.update(t).auxLevel == 2);
    CHECK(!c.ps3ModeActive());
  }
  c.receivePS3(0,0,false,false,23);
  c.receivePS3(255,255,true,true,24,true,true); // Release then press Cross works again.
  state(c,24,ControlSource::None,0,0); CHECK(c.update(24).auxLevel == 0);
  CHECK(c.wifiSession() == 0); CHECK(c.ps3SpeedPercent() == 80);

  const uint32_t fourth = c.claimWiFi(25,1000);
  c.submit(ControlSource::WiFi,51,0,26,1000,1,fourth);
  // Bluetooth liveness expiry/disconnection must not remove the browser's lease.
  state(c,524,ControlSource::WiFi,51,51); CHECK(c.wifiSession() == fourth);
  c.disconnectPS3(); state(c,525,ControlSource::WiFi,51,51);
  CHECK(c.wifiSession() == fourth); CHECK(c.update(525).auxLevel == 1);
  c.receivePS3(0,0,false,true,526); // Disconnect resets Cross edge tracking.
  state(c,526,ControlSource::None,0,0); CHECK(c.wifiSession() == 0);
}

static void crossAfterWatchdogReconnect() {
  // Both a motor-task update and the first returning PS3 packet can notice expiry.
  for (int expireBeforePacket = 0; expireBeforePacket < 2; ++expireBeforePacket) {
    MotionControl c;
    c.receivePS3(0,0,false,true,0);
    const uint32_t session = c.claimWiFi(1,1000);
    c.submit(ControlSource::WiFi,102,0,2,1000,2,session);
    c.receivePS3(0,0,false,true,3); // Holding Cross does not repeatedly stop new owner.
    state(c,502,ControlSource::WiFi,102,102);
    if (expireBeforePacket) {
      state(c,503,ControlSource::WiFi,102,102);
      CHECK(!c.ps3Online(503));
    }
    c.receivePS3(255,255,true,true,503,true,true); // Reconnected Cross must stop immediately.
    state(c,503,ControlSource::None,0,0);
    CHECK(c.update(503).auxLevel == 0); CHECK(c.wifiSession() == 0);
    CHECK(!c.ps3ModeActive()); CHECK(c.ps3Online(503));
  }
}

int main() {
  mixer(); activationAndLatch(); rotationPriority(); ownership();
  stopAndFreshCombo(); timeoutAndDisconnect(); wifiTimeout(); rollover();
  auxiliaryPS3(); auxiliaryWiFi();
  ps3SpeedLevels(); ps3SpeedResetAndRawAPI();
  wifiSessions(); explicitHandover();
  crossAfterWatchdogReconnect();
  printf("PASS 15 groups, %d checks (motion, speed levels, aux, sessions/handover, held Cross/reconnect, stops, timeouts, rollover)\n",checks);
  return 0;
}
