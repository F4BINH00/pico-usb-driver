savedcmd_pico_usb.mod := printf '%s\n'   pico_usb.o | awk '!x[$$0]++ { print("./"$$0) }' > pico_usb.mod
