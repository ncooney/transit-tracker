#/bin/bash

# Gloabal defines
dir="$(dirname -- "$(readlink -f -- "$0")";)";
pid_file="pid_temp.txt"

# Check if this is killing or starting project
if [ "$1" == "--kill" ] || [ "$1" == "-k" ]; then
    # Check the process is running
    if [ -f "${dir}/${pid_file}" ]; then
        # Stop the processes
        echo "[Client]: Stopping MQTT decoder and Flask server"
        kill -9 $(cat "${dir}/${pid_file}") > /dev/null 2>&1
        rm "${dir}/${pid_file}" > /dev/null 2>&1
    else
        # Processes were not running 
        echo "[Client]: MQTT decoder and Flask server are not running"
    fi
else
    # Run the Flask server (updates the webpage)
    echo "[Client]: Starting Flask server, accessible at http://127.0.0.1:5000/"
    flask --app "${dir}/flask_app" run > /dev/null 2>&1 &
    echo $! >> "${dir}/${pid_file}"

    # Run the MQTT decoder (pulls LoRa data from TTN)
    python3 "${dir}/mqtt_decoder.py" > /dev/null 2>&1 &
    echo "[Client]: Starting MQTT decoder"
    echo $! >> "${dir}/${pid_file}"
fi
