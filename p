#!/usr/bin/bash
# to run this file: ./p
#
# nvim tips:
#   file manager: ":Neotree" or ", b f"
#     to get help on Neotree, just press ?
#     toggle between Neotree: Shift-f
#   splits
#     sp: horizontal split
#     vs: vertical split
#     only: keep only this split(close others)
#   switch between source/header: F2
#   search files: ctrl-f, alt-f, C/Tab
#   search strings: ctrl-g, alt-g, /
#   find string/text in whole project: ft <text> | rg <text>
#   folding/unfolding:
#     zM: close all folds.
#     zR: open all folds.
#     za: toggle one fold under cursor.
#     zA: toggle All folds under cursor.
#     zc: close one fold under cursor.
#     zC: close All folds under cursor.
#   documentation:
#     , c c: generate doxygen doc for a class
#     , n f: generate doxygen doc for a function
#     run: doxygen in root of project from a terminal
#   lsp: ,l
#   find/replace:
#     project: ambr <src> <dst> <path>
#     current buffer: :%s/foo/bar/g
#   diff between two files/dirs: delta <DIR1> <DIR2>
#   debuggers: dap in neovim, gf2, gdb
#   find docs of c standard librariy: 
#     - install man-pages-devel and man <method>
#     - zeal
#     - www.devdocs.io 

menu () {
  commands=(
    # main
    "build(debug)" "create patch" "qemu(android x86-64)" "adb connect" "adb install"
    "adb uninstall" "adb run" "adb log" "adb close apk"
    "adb reboot android OS" "adb poweroff android OS"

    # debug
    "clean(debug)"

    # release
    "build(release)" "clean(release)"
  
    # documentation
    "doxygen(generate)" 
    "doxygen(show)" 
 
    # static analyzer
    "cppcheck(generate)"
    "cppcheck(show)"
 
    "frama-c" "frama-c (eva)" "ivette (eva)" "frama-c-gui (eva)"
  
    # code coverage
    "lcov"
    "gcovr"
    "genhtml"
    "show lcov report"
    "kcov(generate)"
    "kcov(show)"
 
    # we can't use lcov, gcov, gcovr(requires: gcc-multilib) with zagros, because these tools asks us to re-compile zagros with:
    #   --coverage -fprofile-arcs -ftest-coverage 
    #   linker flags: -L/usr/lib/gcc/x86_64-unknown-linux-gnu/14.2/32/ -lgcov
    # which is not possible. because there's no libc compiled with zagros. it's statically linked.
    # but we can use them for zagros_test
  
    # trace/profile
    "valgrind(memcheck)" "callgrind" "kcachegrind" "cachegrind" "helgrind" "massif" "ms_print" "drd" "dhat" "dhat(cat)" "bbv" "bbv(cat)"
    "perf"
    "uftrace replay"
    "uftrace record"
    "uftrace dump"
    "uftrace report"
    "uftrace info"
    "uftrace graph"
    "uftrace tui"
  
    # we can't use libasan as compiler flag. (-fsanitize=address) and as linker flag (-L/usr/lib32 -lasan),
    # since zagros is statically linked and this flag needs libc compiled with zagros.
    
    # we can't use uftrace, because of many reasons:
    #   1. it just works with dynamically linked binaries. zagros is statically linked, so uftrace can't work with it.
    #   2. it needs to pass -pg to compiler option, and this requires mcount from clib. but we're building our kernel without libc!
    #   3. even with -finstrument-functions, our binary is still static. so uftrace can't work!
  
    # alternative: manual tracing using this macro for each function:
    #   #define TRACE_ENTER() do { asm volatile("syscall" : : "a"(1), "D"(1), "S"("Entering " __func__ "\n"), "d"(strlen("Entering " __func__ "\n")) : "rcx", "r11", "memory"); } while(0)  // Raw write syscall// Add TRACE_ENTER() at function starts; similar for exit.
  
    # binary analyse
    "ldd" "objdump" "strings" "nm" "readelf" "addr2line" "size" "file" "xxd"
  
    # util
    "scc" "search" "search/replace")
  selected=$(printf '%s\n' "${commands[@]}" | fzf --header="project:")
  
  case $selected in
    "build(debug)")
      ./scripts/build.sh --debug
      if [ $? -eq 1 ]; then
        # error
        mpg123 -f 3000 /home/linarcx/VoidConf/assets/error2.mp3 > /dev/null 2>&1 
      else
        # success
        mpg123 -f 3000 /home/linarcx/VoidConf/assets/success.mp3 > /dev/null 2>&1 
      fi
      ;;
    "create patch")
      read -r -p "Enter name of yoru patch: (it will be save in patches/) " filename
      file="patches/${filename}.json"
      nvim "$file"
      if [ $? -eq 0 ] && [ -s "$file" ]; then
        ./scripts/applyPatch.sh $file
      fi
      ;;
    "qemu(android x86-64)")
      qemu-system-x86_64 \
        -enable-kvm \
        -m 6144 \
        -smp cpus=4 \
        -nic user,hostfwd=tcp:127.0.0.1:5555-:5555 \
        -drive file=~/qemu/android.qcow2,format=qcow2 &
      echo -e "Please first run these commands in Android X86 Shell, before trying to connect adb to it:\nsu\nsetprop service.adb.tcp.port 5555\nstop adbd\nstart adbd"
      ;;
    "adb connect")
      # adb devices
      # adb connect 127.0.0.1:5555
      devices=$(adb devices | awk 'NR > 1 && $2 == "device" {print $1}')

      if [ -z "$devices" ]; then
        printf '\033[31m✗ Error:\033[0m No ADB devices found.\n' >&2
        exit 1
      fi
      
      selected=$(printf '%s\n' "$devices" | fzf \
        --height=40% \
        --layout=reverse \
        --border \
        --prompt='ADB device > ')
      
      if [ -z "$selected" ]; then
        exit 0
      fi
      
      printf '\033[36m→ Connecting to:\033[0m %s\n' "$selected"
      
      adb connect "$selected"
      ;;
    "adb install")
      # unisntall old app first
      echo "--> Uninstalling old app first"
      adb -s 127.0.0.1:5555 uninstall org.linarcx.andFM

      # adb -s 127.0.0.1:5555 install -r "/mnt/D/workspace/c++/active/andFM/build/andFM.apk"
      PROJECT_DIR="/mnt/D/workspace/c++/active/andFM"
      DEBUG_DIR="$PROJECT_DIR/build/debug"
      RELEASE_DIR="$PROJECT_DIR/build/release"
      
      APK_LIST=$(
        find "$DEBUG_DIR" "$RELEASE_DIR" \
          -type f \
          -name '*.apk' \
          2>/dev/null
      )
      
      if [ -z "$APK_LIST" ]; then
        echo "No APK files found in:"
        echo "  $DEBUG_DIR"
        echo "  $RELEASE_DIR"
        exit 1
      fi
      
      APK=$(printf '%s\n' "$APK_LIST" | fzf \
        --height=40% \
        --layout=reverse \
        --border \
        --prompt="Select APK: " \
        --header="Choose an APK to install")
      
      if [ -z "$APK" ]; then
        echo "Installation cancelled."
        exit 0
      fi
      
      echo "--> Installing:"
      echo "  $APK"
      
      adb -s 127.0.0.1:5555 install -r "$APK"

      echo "--> Running andFM..."
      adb -s 127.0.0.1:5555 shell am start -n org.linarcx.andFM/android.app.NativeActivity
      ;;
    "adb uninstall")
      adb -s 127.0.0.1:5555 uninstall org.linarcx.andFM
      ;;
    "adb run")
      adb -s 127.0.0.1:5555 shell am start -n org.linarcx.andFM/android.app.NativeActivity
      ;;
    "adb log")
      adb -s 127.0.0.1:5555 logcat | grep andFM
      ;;
    "adb close apk")
      adb -s 127.0.0.1:5555 shell am force-stop org.linarcx.andFM
      ;;
    "adb reboot android OS")
      adb -s 127.0.0.1:5555 shell reboot -p
      ;;
    "adb poweroff android OS")
      adb -s 127.0.0.1:5555 shell reboot --poweroff
      ;;
    "clean(debug)")
      echo ">>> cleaning build/debug directory"
      ./scripts/build.sh --clean --debug
      if [ $? -eq 1 ]; then
        # error
        mpg123 -f 3000 /home/linarcx/VoidConf/assets/error2.mp3 > /dev/null 2>&1 
      else
        # success
        mpg123 -f 3000 /home/linarcx/VoidConf/assets/success.mp3 > /dev/null 2>&1 
      fi
      ;;
    "build(release)")
      ./scripts/build.sh --release 
      if [ $? -eq 1 ]; then
        # error
        mpg123 -f 3000 /home/linarcx/VoidConf/assets/error2.mp3 > /dev/null 2>&1 
      else
        # success
        mpg123 -f 3000 /home/linarcx/VoidConf/assets/success.mp3 > /dev/null 2>&1 
      fi
     ;;
    "clean(release)")
      ./scripts/build.sh --clean --release
      if [ $? -eq 1 ]; then
        # error
        mpg123 -f 3000 /home/linarcx/VoidConf/assets/error2.mp3 > /dev/null 2>&1 
      else
        # success
        mpg123 -f 3000 /home/linarcx/VoidConf/assets/success.mp3 > /dev/null 2>&1 
      fi
      ;;
    "doxygen(generate)")
      doxygen
      ;;
    "doxygen(show)")
      ~/software/brave/brave-browser-1.86.142-linux-amd64/brave ./docs/doxygen/html/index.html
      ;;
    "cppcheck")
      rm report/*
      # --suppressions-list=suppress.txt 
      # Suppress a specific warning. The format of <spec> is: [error id]:[filename]:[line].
      # - The [filename] and [line] are optional.
      # - [error id] may be * to suppress all warnings (for a specified file or files).
      # - [filename] may contain the wildcard characters * or ?.
      cppcheck --addon=cppcheck/misra.json --addon=cppcheck/findcasts.json --addon=cppcheck/misc.json --addon=cppcheck/y2038.json --addon=cppcheck/threadsafety.json --inline-suppr --std=c11 --enable=all --error-exitcode=1 --platform=unix64 --report-type=misra-c-2012 -q --xml --xml-version=2 lib/util/*.c lib/*.c example/*.c -I lib/util/ > report/cppcheck.xml 2>&1
      cppcheck-htmlreport --file=report/cppcheck.xml --title="andFM" --report-dir=report --source-dir=.
      ;;
    "cppcheck(show)")
      ~/software/brave/brave-browser-1.86.142-linux-amd64/brave ./report/index.html
      #xdg-open report/index.html
      ;;
    "frama-c")
      frama-c -json-compilation-database compile_commands.json example/* lib/* lib/util/*
      ;;
    "frama-c (eva)")
      frama-c -json-compilation-database compile_commands.json example/* lib/* lib/util/* -eva -eva-precision 11
      ;;
    "ivette (eva)")
      ivette -json-compilation-database compile_commands.json example/* lib/* lib/util/* -eva -eva-precision 11
      ;;
    "frama-c-gui (eva)")
      frama-c-gui -json-compilation-database compile_commands.json example/* lib/* lib/util/* -eva -eva-precision 11
      ;;
    "lcov")
      # https://wiki.cs.jmu.edu/student/gcov/start
      ./build/debug/andFM
      lcov --capture --directory build/debug --output-file build/debug/coverage.info
      ;;
    "gcovr")
      gcovr build/debug
      ;;
    "genhtml")
      genhtml build/debug/coverage.info --output-directory build/debug/coverage_report
      ;;
    "show lcov report")
      ~/software/brave/brave-browser-1.86.142-linux-amd64/brave ./build/debug/coverage_report/index.html
      # xdg-open unit_test/build/coverage_report/index.html
      ;;
    "kcov(generate)")
      rm -r coverage/*
      kcov coverage/ build/debug/andFM
      ;;
    "kcov(show)")
      ~/software/brave/brave-browser-1.86.142-linux-amd64/brave ./coverage/index.html
      # xdg-open coverage/index.html
      ;;
    "llvm-cov")
      ;;
    "valgrind(memcheck)")
      valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all --track-origins=yes --xtree-leak=yes -s -v build/debug/andFM
      ;;
    "callgrind")
      valgrind --tool=callgrind --dump-instr=yes --collect-jumps=yes -s -v build/debug/andFM
      ;;
    "kcachegrind")
      ls callgrind.out.* cachegrind.out.* | fzf --header="kcachgrind: " | xargs kcachegrind
      ;;
    "cachegrind")
      valgrind --tool=cachegrind --cache-sim=yes --branch-sim=yes -s -v build/debug/andFM
      ;;
    "helgrind")
      valgrind --tool=helgrind -s -v build/debug/andFM
      ;;
    "massif")
      valgrind --tool=massif -s -v build/debug/andFM
      ;;
    "ms_print")
      ls massif.out.* | fzf --header="ms_print: " | xargs ms_print
      ;;
    "drd")
      valgrind --tool=drd --trace-fork-join=yes --trace-mutex=yes --trace-semaphore=yes -s -v build/debug/andFM
      ;;
    "dhat")
      valgrind --tool=dhat -s -v build/debug/andFM
      ;;
    "dhat(cat)")
      ls dhat.out.* | fzf --header="dhat: " | xargs cat | less
      ;;
    "bbv")
      valgrind --tool=exp-bbv -s -v build/debug/andFM
      ;;
    "bbv(cat)")
      ls bb.out.* | fzf --header="bbv: " | xargs cat | less
      ;;
    "perf")
      ls build/debug | fzf --header="perf: " | xargs perf stat -d
      ;;
    "uftrace record(test)")
      uftrace --srcline -a --symbols --time --no-libcall --symbols -F __clove_symint___UtilSuite* record build/debug/andFM
      ;;
    "uftrace replay(tests)")
      uftrace replay
      ;;
    "uftrace dump(test)")
      uftrace dump --chrome > dump.json
      echo "load dump.json in chrome://tracing/"
      ;;
    "uftrace report(test)")
      uftrace report
      ;;
    "uftrace info(test)")
      uftrace info
      ;;
    "uftrace graph(test)")
      uftrace graph
      ;;
    "uftrace tui(test)")
      uftrace tui
      ;;
    "lttng")
      ;;
    "gprof")
      ;;
    "ldd")
      ls build/*.bin build/obj/* | fzf --header="objdump: " | xargs ldd | less -N
      ;;
    "objdump")
      ls build/*.bin build/obj/* | fzf --header="objdump: " | xargs objdump -x -d -p -f -a -t -r | less -N
      ;;
    "strings")
      ls build/debug/* | fzf --header="strings: " | xargs strings | less -N
      ;;
    "nm")
      ls build/debug/* | fzf --header="nm: " | xargs nm -l -n --synthetic | less -N
      ;;
    "readelf")
      ls build/*.bin build/obj/* | fzf --header="readelf: " | xargs readelf -a -h -g -t -s -d | less -N
      ;;
    "addr2line")
      file=$(ls build/*.bin build/obj/* | fzf --header="Select ELF file for addr2line")
      echo "Enter the memory address:"; read -r addr;
      addr2line -f -a -e $file $addr
      ;;
    "size")
      ls build/*.bin build/obj/* | fzf --header="size: " | xargs size --common -t -d -A | less -N
      ;;
    "file")
      ls build/*.* build/obj/* | fzf --header="file type: " | xargs file
      ;;
    "xxd")
      ls build/*.bin build/*.iso build/obj/* unit_test/build/*.o | fzf --header="xxd: " | xargs xxd | less
      ;;
    "scc")
      scc -p -a -u -i c,h,md,cpp,c++,hpp,txt,json,s,ld
      ;;
    "search")
      read -p "keyword: " p_keyword; rg "$p_keyword" ;;
    "search/replace")
      read -p "to_search: " to_search
      read -p "to_replace: " to_replace
      ambr "$to_search" "$to_replace" ;;
    "elfedit")
      ;;
    "strip")
      ;;
    "objcopy")
      ;;
    "ranlib")
      ;;
    "gcc-ar")
      ;;
    "gcc-nm")
      ;;
    "gcc-ranlib")
      ;;
    "gcov")
      ;;
    "gcov-dump")
      ;;
    "gcov-tool")
      ;;
    "gprof")
      ;;
    "gprofng")
      ;;
    "gprofng-archive")
      ;;
    "gprofng-collect-app")
      ;;
    "gprofng-display-html")
      ;;
    "gprofng-display-src")
      ;;
    "gprofng-display-text")
     ;;
    *) ;;
  esac
}

if [ $# -eq 0 ]; then
  menu
fi

"$@"
