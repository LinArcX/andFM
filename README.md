# Enable ADB over network. If the Android-x86 build doesn't automatically enable it, open a terminal in Android and run:
  su
  setprop service.adb.tcp.port 5555
  stop adbd
  start adbd
