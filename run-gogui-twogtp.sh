#!/bin/sh
echo "GoGui-TwoGTP Launcher V20221101"
# commands for player 1
P1B='./nogo/nogo --shell --name="toooot" --black="search=MCTS thread=4" --white="search=Random"' 
P1W='./nogo/nogo --shell --name="toooot" --white="search=MCTS thread=4" --black="search=Random"'
# commands for local player 2
P2B='./nogo-judge --shell --name="Judge-Weak-Black" --black="strong"'
P2W='./nogo-judge --shell --name="Judge-Weak-White" --white="strong"'
# commands for remote player 2
# P2B="gogui-client tcglinux8 10002"
# P2W="gogui-client tcglinux8 10002"

# other settings
games=${1:-10} # total games to play
timelimit=40 # total thinking time in second
configs="-size 9 -komi 0 -auto -games $((games/2)) -verbose" # gogui-twogtp

# set up environment
cleanup() { # when program exit
	kill $(jobs -p) 2>/dev/null # kill all background processes
	rm $monitor 2>/dev/null # this will terminate monitor script
}
trap 'echo Interrupted; cleanup; exit 0;' INT
stamp=gogui-twogtp-$(date +"%Y%m%d%H%M%S")
nobuf="stdbuf -o0"
if [ -d gogui-1.4.9/bin ]; then
    chmod +x gogui-1.4.9/bin/*
    export PATH=$PATH:$(realpath gogui-1.4.9/bin)
fi
for cmd in java python3 gogui-twogtp gogui-client ./nogo-judge; do
	if ! command -v $cmd >/dev/null; then
		echo "Requirement $cmd is missing" >&2
		exit 100
	fi
done

# print details of both sides
echo "=========== PLAYERS ==========="
echo "P1B: $P1B"
echo "P1W: $P1W"
echo "P2B: $P2B"
echo "P2W: $P2W"

# launch gogui-twogtp and generate results
echo "============ GAMES ============"
result() { # for obtaining game results from twogtp .dat file
	RES_R=($(grep -v ^\# $1 | cut -f4))
	TIME_B=($(grep -v ^\# $1 | cut -f8))
	TIME_W=($(grep -v ^\# $1 | cut -f9))
	N=$(grep -v ^\# $1 | wc -l)
	BW=0
	WW=0
	IA_RESULT=()
	TLE_RESULT=()
	for (( i=0; i<$N; i++ )); do
		IA=(X B W)
		grep -Eo ";[BW]\[[a-i][a-i]\]" ${1%.dat}-$i.sgf | ./nogo-judge --check >/dev/null
		IA=${IA[$?]}
		[ $IA != X ] && IA_RESULT+=("$IA#$i")
		TLE_B=$(python3 -c "print(int(${TIME_B[$i]} > $timelimit))")
		TLE_W=$(python3 -c "print(int(${TIME_W[$i]} > $timelimit))")
		(( $TLE_B )) && TLE_RESULT+=("B#$i")
		(( $TLE_W )) && TLE_RESULT+=("W#$i")

		if [ "${RES_R[$i]}" == "B+R" ]; then
			if [ $IA != B ] && [ $TLE_B != 1 ]; then
				BW=$((BW+1)) # black win
			elif [ $IA != W ] && [ $TLE_W != 1 ]; then
				WW=$((WW+1)) # black violates the rules --> white win
			fi
		elif [ "${RES_R[$i]}" == "W+R" ]; then
			if [ $IA != W ] && [ $TLE_W != 1 ]; then
				WW=$((WW+1)) # white win
			elif [ $IA != B ] && [ $TLE_B != 1 ]; then
				BW=$((BW+1)) # white violates the rules --> black win
			fi
		fi
	done
	echo "$BW:$WW"
	(( ${#IA_RESULT[@]} ))  && echo "> IA: ${IA_RESULT[@]}"
	(( ${#TLE_RESULT[@]} )) && echo "> TLE: ${TLE_RESULT[@]}"
	RESULT=($BW $WW $N)
}
mon-states() { # for monitoring log and print states
	log=$1
	mon=${log%.log}.txt
	touch $log $mon
	Gidx=0
	seq=(X)
	tail -f $log | $nobuf grep -Ev "^[BW]<< ([^=].+)?$" | \
	$nobuf grep -E "genmove|clear_board|quit" -A 1 | \
	$nobuf grep -Ev "genmove|--" | $nobuf cut -b7- | \
	$nobuf sed "s/[^A-Z0-9]//g" | while read move; do
		if [ "$move" ]; then
			seq+=($move)
			# { <<< "${seq[@]}" ./nogo-judge --print >/dev/null; } |& \
			# 	tail -n12 # | sed 's/\xB7/\xA0/g'
			echo
		elif (( ${#seq[@]} )); then
			[ $Gidx -ge $((games/2)) ] && { pkill -f "tail -f $log"; exit; }
			echo =====================
			echo $FLAG${FLAG:+: }Game $((Gidx++))
			echo =====================
			echo
			seq=()
		fi
	done > $mon &
}

# display info
echo "Storage: $stamp"
mkdir -p "$stamp" || exit 1
monitor=$stamp.mon
echo "Monitor: ./$monitor"
{ # write monitor script
	echo '#!/bin/bash'
	echo "trap 'kill \$(jobs -p) 2>/dev/null; exit 0;' INT"
	echo "touch $stamp/P1B_P2W.txt $stamp/P2B_P1W.txt"
	echo "tail -f $stamp/P1B_P2W.txt &"
	echo "tail -f $stamp/P2B_P1W.txt &"
	echo "while [ -e $monitor ]; do sleep 1; done"
	echo 'kill $(jobs -p) 2>/dev/null'
} > $monitor
chmod +x $monitor

# P1 plays black, P2 plays white
echo -n "P1B vs P2W: "
FLAG="P1B vs P2W" mon-states $stamp/P1B_P2W.log
gogui-twogtp -black "$P1B" -white "$P2W" -sgffile $stamp/P1B_P2W $configs 2>&1 | tee $stamp/P1B_P2W.log | \
	$nobuf grep ^Game | $nobuf sed "s/.*/#/g" | { $nobuf tr -d '\n'; echo -n " "; } >&2
result $stamp/P1B_P2W.dat
P1P2=(${RESULT[@]})

# P2 plays black, P1 plays white
echo -n "P2B vs P1W: "
FLAG="P2B vs P1W" mon-states $stamp/P2B_P1W.log
gogui-twogtp -black "$P2B" -white "$P1W" -sgffile $stamp/P2B_P1W $configs 2>&1 | tee $stamp/P2B_P1W.log | \
	$nobuf grep ^Game | $nobuf sed "s/.*/#/g" | { $nobuf tr -d '\n'; echo -n " "; } >&2
result $stamp/P2B_P1W.dat
P2P1=(${RESULT[@]})

# print results
echo "=========== RESULTS ==========="
echo "P1: (${P1P2[0]}+${P2P1[1]})/$((${P1P2[2]}+${P2P1[2]}))" \
     "= $(python3 -c "print(round($((${P1P2[0]}+${P2P1[1]}))*100/$((${P1P2[2]}+${P2P1[2]})),2))")%"
echo "P2: (${P2P1[0]}+${P1P2[1]})/$((${P2P1[2]}+${P1P2[2]}))" \
     "= $(python3 -c "print(round($((${P2P1[0]}+${P1P2[1]}))*100/$((${P2P1[2]}+${P1P2[2]})),2))")%"

# done! clean up
cleanup
