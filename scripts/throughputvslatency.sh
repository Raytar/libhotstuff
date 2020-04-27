#!/usr/bin/env bash

run_benchmark() {
	mkdir -p "$1"
	ansible-playbook -i scripts/hotstuff.gcp.yml scripts/throughputvslatency.yml \
		-e "destdir='$1' rate='$2' batch_size='$3' payload='$4' time='$5' num_clients='$6'"
}

# Exit if anything fails
set -e

basedir="$1/throughputvslatency-$(date +%Y%m%d%H%M%S)"
mkdir -p "$basedir"

# how many times to repeat each benchmark
for n in {1..1}; do

	batch=100
	payload=0
	for t in {5000,6000,8000,10000,10500,11000,11500,12000,12500,13000}; do
		run_benchmark "$basedir/lhs-b$batch-p$payload/$n/t$t" "$t" "$batch" "$payload" 10 2
	done

	batch=400
	payload=0
	for t in {40000,50000,60000,70000,80000,90000,100000,110000,120000}; do
		run_benchmark "$basedir/lhs-b$batch-p$payload/$n/t$t" "$t" "$batch" "$payload" 10 3
	done

	batch=800
	payload=0
	for t in {30000,40000,50000,60000,70000,80000,90000,100000,110000,120000,140000}; do
		run_benchmark "$basedir/lhs-b$batch-p$payload/$n/t$t" "$t" "$batch" "$payload" 10 4
	done

done
