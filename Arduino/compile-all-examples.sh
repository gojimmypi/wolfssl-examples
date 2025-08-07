#!/bin/bash
#
# ./wolfssl-arduino.sh INSTALL
# export ARDUINO_ROOT=/home/$USER/Arduino/libraries
#
# ./wolfssl-arduino.sh INSTALL  /mnt/c/Users/gojimmypi/Documents/Arduino/libraries

# Run shell check to ensure this a good script.
# Specify the executable shell checker you want to use:
MY_SHELLCHECK="shellcheck"

# Check if the executable is available in the PATH
if command -v "$MY_SHELLCHECK" >/dev/null 2>&1; then
    # Run your command here
    $MY_SHELLCHECK "$0" || exit 1
else
    echo "$MY_SHELLCHECK is not installed. Please install it if changes to this script have been made."
    exit 1
fi

set +e
BOARD_COMPILE_CT=0
BOARD_SKIP_CT=0

# need to reassign ARDUINO_ROOT in this run
ARDUINO_ROOT="$HOME/Arduino/libraries"

ICON_OK=$(printf "\xE2\x9C\x85")
ICON_FAIL=$(printf "\xE2\x9D\x8C")
HAS_NETWORK=""
HAS_MEMORY=""

echo "Icon check"
printf 'OK:   %s' "$ICON_OK"
printf 'Fail: %s' "$ICON_FAIL"

echo "***************************************************************"
echo "Installed libraries:"
echo "***************************************************************"
ls  "$ARDUINO_ROOT" -al

echo "***************************************************************"
echo "wolfssl:"
echo "***************************************************************"
ls  "$ARDUINO_ROOT/wolfssl" -al

echo "***************************************************************"
echo "wolfssl/src:"
echo "***************************************************************"
ls  "$ARDUINO_ROOT/wolfssl/src/" -al

echo "***************************************************************"
ls  "$ARDUINO_ROOT/wolfssl/src/user_settings.h" -al
echo "***************************************************************"
cat "$ARDUINO_ROOT/wolfssl/src/user_settings.h"
ls "$ARDUINO_ROOT" -al

echo "Begin compile"
SUCCESS=true


while IFS= read -r BOARD; do
    BOARD="${BOARD//$'\r'/}"  # Remove carriage returns from the line

    if [[ "$BOARD" =~ ^[[:space:]]*$ ]]; then
        continue #skip blank lines
    fi

    if [[ "$BOARD" == \#* || "$BOARD" == " "*#* ]]; then
        continue  # Skip comments and lines starting with space + #
    fi

    echo "Compiling for $BOARD"
    while IFS= read -r EXAMPLE; do
        echo ""
        echo "*************************************************************************************"
        echo "Begin $EXAMPLE for $BOARD"
        echo "*************************************************************************************"

        # Assume no network unless otherwise known
        HAS_NETWORK="false"
        HAS_MEMORY="false"

        if [[ "$BOARD" =~ ^esp32:esp32:[^[:space:]]+$ ]]; then
            HAS_NETWORK="true"
            HAS_MEMORY="true"
        fi

        # No WiFi on ESP32-H2
        if [[ "$BOARD" =~ ^esp32:esp32:(esp32h2)$ ]]; then
            HAS_NETWORK="false"
        fi

        if [[ "$BOARD" =~ ^arduino:avr:(uno|mega|nano)$ ]]; then
            echo "AVR"
        fi

        if [[ "$BOARD" =~ ^arduino:avr:(mega)$ ]]; then
            echo "AVR"
            HAS_MEMORY="true"
        fi
        if [[ "$BOARD" == "arduino:avr:uno" ]]; then
            echo "Skipping $EXAMPLE for $BOARD needs updated code - see https://github.com/wolfSSL/Arduino-wolfSSL/issues/14"
        fi

        # skip known no-wifi SAMD boards
        if [[ "$BOARD" =~ "arduino:samd:arduino_zero_native" && ( "$EXAMPLE" =~ "wolfssl_server" || "$EXAMPLE" =~ "wolfssl_client" || "$EXAMPLE" =~ "test" ) ]]; then
            echo "Skipping $EXAMPLE for $BOARD (No WiFi support)"
        fi

        # Global variables use 2682 bytes (130%) of dynamic memory, leaving -634 bytes for local variables. Maximum is 2048 bytes.
        # AES const saved in AVR RAM, not flash.
        if [[ "$BOARD" == "arduino:avr:uno" && "$EXAMPLE" == *wolfssl_AES_CTR ]]; then
            echo "Skipping $EXAMPLE for $BOARD needs updated code - see https://github.com/wolfSSL/Arduino-wolfSSL/issues/14"
        fi

        if [[ "$BOARD" == "arduino:avr:nano" && "$EXAMPLE" == *wolfssl_AES_CTR ]]; then
            echo "Skipping $EXAMPLE for $BOARD needs updated code - see https://github.com/wolfSSL/Arduino-wolfSSL/issues/14"
            ((BOARD_SKIP_CT++))
        fi


        case "$EXAMPLE" in
            *template)
                echo "All boards supported for template example"
                ;;

            *wolfssl_AES_CTR)
                if [[ "$HAS_MEMORY" != "true" ]]; then
                    echo "Skipping $EXAMPLE for $BOARD (Not enough memory)"
                    ((BOARD_SKIP_CT++))
                    continue
                fi
                ;;

            *wolfssl_client)
                if [[ "$HAS_MEMORY" != "true" ]]; then
                    echo "Skipping $EXAMPLE for $BOARD (Not enough memory)"
                    ((BOARD_SKIP_CT++))
                    continue
                fi
                if [[ "$HAS_NETWORK" != "true" ]]; then
                    echo "Skipping $EXAMPLE for $BOARD (No network capability)"
                    ((BOARD_SKIP_CT++))
                    continue
                fi
                ;;

            *wolfssl_client_dtls)
                if [[ "$HAS_MEMORY" != "true" ]]; then
                    echo "Skipping $EXAMPLE for $BOARD (Not enough memory)"
                    ((BOARD_SKIP_CT++))
                    continue
                fi
                ;;

            *wolfssl_server)
                if [[ "$HAS_MEMORY" != "true" ]]; then
                    echo "Skipping $EXAMPLE for $BOARD (Not enough memory)"
                    ((BOARD_SKIP_CT++))
                    continue
                fi
                ;;

            *wolfssl_server_dtls)
                if [[ "$HAS_MEMORY" != "true" ]]; then
                    echo "Skipping $EXAMPLE for $BOARD (Not enough memory)"
                    ((BOARD_SKIP_CT++))
                    continue
                fi
                ;;

            *wolfssl_version)
                echo "All boards supported for template example"
                ;;

            *)  # else
                echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
                echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"

                echo "Default handling for new example: $EXAMPLE"
                # TODO: Do not let examples fall though here, add checks above.
                echo "Check for failed messages; add explicit support above"

                echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
                echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
                ;;
        esac

        # skip known no-wifi AVR boards
        if [[ "$BOARD" =~ ^arduino:avr:(uno|mega|nano)$ ]] && \
        { [[ "$EXAMPLE" =~ "wolfssl_server" ]] || \
            [[ "$EXAMPLE" =~ "wolfssl_client" ]] || \
            [[ "$EXAMPLE" =~ "wolfssl_server_dtls" ]] || \
            [[ "$EXAMPLE" =~ "wolfssl_client_dtls" ]] || \
            [[ "$EXAMPLE" =~ "test" ]]; }; then
            echo "Skipping $EXAMPLE for $BOARD (No WiFi support)"
            ((BOARD_SKIP_CT++))
            continue
        fi

        # skip known no-wifi teensy AVR boards
        if [[ "$BOARD" =~ ^teensy:avr:(teensy40)$ ]] && \
        { [[ "$EXAMPLE" =~ "wolfssl_server" ]] || \
            [[ "$EXAMPLE" =~ "wolfssl_client" ]] || \
            [[ "$EXAMPLE" =~ "wolfssl_server_dtls" ]] || \
            [[ "$EXAMPLE" =~ "wolfssl_client_dtls" ]] || \
            [[ "$EXAMPLE" =~ "test" ]]; }; then
            echo "Skipping $EXAMPLE for $BOARD (needs ethernet update)"
            ((BOARD_SKIP_CT++))
            continue
        fi

        # skip examples other than template and version for known tiny memory boards
        if  [[ "$BOARD" =~ ( "arduino:avr:uno"|"arduino:avr:nano" ) && ( "$EXAMPLE" != "template" ) && ( "$EXAMPLE" != "wolfssl_version" ) ]]; then
            echo "Skipping $EXAMPLE for $BOARD (memory limited)"
            ((BOARD_SKIP_CT++))
            continue
        fi


        # TODO skip ESP8266
        if [[ "$BOARD" =~ "esp8266:esp8266:generic"  && ( "$EXAMPLE" != "template" ) && ( "$EXAMPLE" != "wolfssl_version" ) ]]; then
            echo "Skipping $EXAMPLE for $BOARD (network memory constraint testing)"
            ((BOARD_SKIP_CT++))
            continue
        fi

        # If otherwise not excluded, compile this $EXAMPLE for this $BOARD
        ((BOARD_COMPILE_CT++))
        echo "-------------------------------------------------------------------------------------"
        echo "Compiling $EXAMPLE for $BOARD"
        echo "-------------------------------------------------------------------------------------"
        echo "arduino-cli compile --fqbn \"$BOARD\" \"$EXAMPLE\""
              arduino-cli compile --fqbn  "$BOARD"   "$EXAMPLE"
        EXIT_CODE=$?

        if [ "$EXIT_CODE" -ne 0 ]; then
            echo "$ICON_FAIL Compilation failed for $EXAMPLE on $BOARD (Exit code: $EXIT_CODE)"
            SUCCESS=false
        else
            echo "$ICON_OK Compilation succeeded for $EXAMPLE on $BOARD"
        fi
    done < <(find ./sketches -mindepth 1 -maxdepth 1 -type d) # for each EXAMPLE directory name
done < board_list.txt # for each BOARD

if [ "$SUCCESS" = true ]; then
    echo "$ICON_OK All $BOARD_COMPILE_CT sketches compiled successfully! $BOARD_SKIP_CT boards skipped"
else
    echo "$ICON_FAIL One or more sketches failed to compile."
    exit 1
fi