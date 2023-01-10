#!/bin/bash
echo "GoGui-Server Launcher V20211112"
# command to launch
player='./nogo/nogo --shell --name="toooot" --black="search=MCTS simulation=8500" --white="search=MCTS simulation=8500'
# port for binding
port=${1:-auto}
# display mode: states or commands
display=states

# set up environment
cleanup() { # when program exit
    kill $(jobs -p) 2>/dev/null # kill all background processes
    rm $monitor 2>/dev/null # this will terminate monitor script
}
trap 'echo Interrupted; cleanup; exit 0;' INT
stamp=gogui-server-$(date +"%Y%m%d%H%M%S")
nobuf="stdbuf -o0"
if [ -d gogui-1.4.9/bin ]; then
	chmod +x gogui-1.4.9/bin/*
	export PATH=$PATH:$(realpath gogui-1.4.9/bin)
fi
for cmd in java gogui-server curl ./nogo-judge; do
	if ! command -v $cmd >/dev/null; then
		echo "Requirement $cmd is missing" >&2
		exit 100
	fi
done

# create monitor script
mon-states-on-the-fly() { # for monitoring log and print states
	seq=(X)
	$nobuf grep -E "^play|^= [A-Z][0-9]|clear_board" | \
	$nobuf sed -e "s/play ./=/g" -e "s/clear_board//g" | $nobuf cut -b3- | \
	while read move; do
		if [ "$move" ]; then
			seq+=($move)
			{ <<< "${seq[@]}" ./nogo-judge --print >/dev/null; } |& \
				tail -n12 # | sed 's/\xB7/\xA0/g'
			echo
		elif (( ${#seq[@]} )); then
			echo =====================
			echo $FLAG${FLAG:+: }Game $((Gidx++))
			echo =====================
			echo
			seq=()
		fi
	done
}
if [ "$display" == "states" ]; then # display states instead of commands
	show() { mon-states-on-the-fly | tee $stamp.txt; }
	monitor=
else # display commands instead of states
	show() { tee >(mon-states-on-the-fly > $stamp.txt); }
	monitor=$stamp.mon
	{ # write monitor script
		echo '#!/bin/bash'
		echo "trap 'kill \$(jobs -p) 2>/dev/null; exit 0;' INT"
		echo "tail -f $stamp.txt &"
		echo "while [ -e $monitor ]; do sleep 1; done"
		echo 'kill $(jobs -p) 2>/dev/null'
	} > $monitor
	chmod +x $monitor
fi

# launch gogui-server
echo "==============================="
echo "Program: $player"
[ "$monitor" ] && echo "Monitor: ./$monitor"
echo "==============================="
echo "Host: $(hostname) ($(curl -s --ipv4 ifconfig.io))"
[[ $port =~ [0-9]+ ]] || port=10000
touch $stamp.{log,txt}
while (( $port )); do
	echo "Port: $port"
	gogui-server -port $port -loop -verbose "$player" 2>&1 | tee $stamp.log | show
	if [ "$(tail -n1 "$stamp.log")" == "Address already in use (Bind failed)" ]; then
		port=$((port+1))
	else
		port=0
	fi
done

cleanup
