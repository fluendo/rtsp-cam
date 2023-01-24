#!/bin/bash -e

frames_dir=frames
gstlog_file=gst.log
pid=0


prepare() {
	cmake --build build

	rm -fv n
	rm -fv "${gstlog_file}"
	rm -fv "screeenshot_*"
	rm -fr "${frames_dir}"
	mkdir "${frames_dir}"
}

start_rtsp_cam() {
	prepare

	trap stop_rtsp_cam EXIT

	GST_DEBUG=8 \
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
	grep 'rec queue level' "${gstlog_file}" \
	| sed -e 's/^\([0-9:.]*\).*rec queue level: \([0-9]*\) bytes$/\1 \2/' \
	| tee n \
	| gnuplot -p -e "set terminal dumb size 120, 50; set xdata time; set format x '%M:%.2S'; set timefmt '%H:%M:%S'; set autoscale; plot '-' using 1:2 with linespoint"
}

pts_list_from_identity() {
	grep "basetransform.*$1.*PTS" "${gstlog_file}" \
	| sed -e 's/^\([0-9:.]*\).*PTS \([0-9:.]*\).*$/\1 \2/'
}

plot_buffers_pts() {
	rm -f enc rec
	pts_list_from_identity "enc_identity" > enc
	pts_list_from_identity "rec_identity" > rec
	gnuplot -p -e "set terminal dumb size 120, 50; set xdata time; set ydata time; set format y '%M:%.9S'; set format x '%M:%.2S'; set timefmt '%H:%M:%S'; set autoscale; plot 'enc' using 1:2 with lines, 'rec' using 1:2 with lines"
}

#start_rtsp_cam
#test_screenshots
#test_recording
#stop_rtsp_cam

#plot_appsrc_queue
plot_buffers_pts

exit 0
