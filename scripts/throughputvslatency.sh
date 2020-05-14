#!/usr/bin/env bash

run_benchmark() {
	mkdir -p "$1"
	ansible-playbook -i scripts/hotstuff.gcp.yml scripts/throughputvslatency.yml \
		-e "destdir='$1' rate='$2' batch_size='$3' payload='$4' max_async='$5' time='$6' num_clients='$7' "
}

# Exit if anything fails
set -e

basedir="$1/throughputvslatency-$(date +%Y%m%d%H%M%S)"
mkdir -p "$basedir"

# how many times to repeat each benchmark
for n in {1..5}; do

	batch=100
	payload=0
	for t in {2500,3000,3250,3500,3750,4000,4270,4500,4750,5000,5500}; do
		run_benchmark "$basedir/lhs-b$batch-p$payload/$n/t$t" "$t" "$batch" "$payload" 75 60 4
	done

	batch=200
	payload=0
	for t in {6000,7000,8000,9000,10000,11000,12000,13000,14000,15000}; do
		run_benchmark "$basedir/lhs-b$batch-p$payload/$n/t$t" "$t" "$batch" "$payload" 150 60 4
	done

	batch=400
	payload=0
	for t in {10000,15000,20000,25000,27500,30000,32500,35000,37500,40000,42500}; do
		run_benchmark "$basedir/lhs-b$batch-p$payload/$n/t$t" "$t" "$batch" "$payload" 300 60 4
	done

	# Payload tests. Need to recompile and redeploy binary

	# batch=200
	# payload=128
	# for t in {6000,7000,8000,9000,10000,11000,12000,13000,14000,15000}; do
	# 	run_benchmark "$basedir/lhs-b$batch-p$payload/$n/t$t" "$t" "$batch" "$payload" 300 60 4
	# done

	# batch=200
	# payload=1024
	# for t in {6000,8000,9000,10000,10500,11000,11250,11500,11750,12000,12500}; do
	# 	run_benchmark "$basedir/lhs-b$batch-p$payload/$n/t$t" "$t" "$batch" "$payload" 300 60 4
	# done

done
