#!/usr/bin/env bash

# Exit if anything fails
set -e

basedir="$1/throughputvslatency-$(date +%Y%m%d%H%M%S)"

run_benchmark_series() {
	for n in {1..1}; do
		rundir="$basedir/b$1-p$2/$n"
		mkdir -p "$rundir"
		for t in {2,5,8,10,15,25,40,60,100}; do
			testdir="$rundir/t$t"
			mkdir -p "$testdir"
			ansible-playbook -i scripts/hotstuff.gcp.yml scripts/throughputvslatency.yml \
				-e "destdir='$testdir' rate='$t' batch_size='$1' payload='$2' time='10'"
		done
	done
}

mkdir -p $basedir

run_benchmark_series 100 0
run_benchmark_series 400 0
run_benchmark_series 800 0
