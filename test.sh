#!/bin/bash -e

frames_dir=frames
gstlog_file=gst.log
pid=0


prepare() {
	cmake --build build

	rm -fv "${gstlog_file}"
	rm -fv "screeenshot_*"
	rm -fr "${frames_dir}"
	mkdir "${frames_dir}"
}

start_rtsp_cam() {
	prepare

	trap stop_rtsp_cam EXIT

	GST_DEBUG=5 \
	GST_DEBUG_NO_COLOR=1 \
	GST_DEBUG_FILE="${gstlog_file}" \
	./build/rtsp-cam &
	pid="$!"
	sleep 1
}

stop_rtsp_cam() {
	[ "${pid}" -ne 0 ] || return
	kill "${pid}"
	pid=0
}

test_screenshots() {
	kill -s USR1 "${pid}"
	sleep 0.5
	kill -s USR1 "${pid}"
	kill -s USR1 "${pid}"
	sleep 0.5
}

test_recording() {
	sleep 1
	kill -s USR2 "${pid}"
	sleep 3
	kill -s USR2 "${pid}"
	sleep 1
}

plot_appsrc_queue() {
	grep 'Currently queued' "${gstlog_file}" \
	| grep -o '[0-9]* buffers' \
	| cut -d' ' -f 1 \
	| gnuplot -p -e "plot '-' with linespoint"
}

start_rtsp_cam
#test_screenshots
test_recording
stop_rtsp_cam

plot_appsrc_queue

exit 0
