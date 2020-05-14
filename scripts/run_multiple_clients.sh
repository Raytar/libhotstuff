#!/usr/bin/env bash

if [ "$#" -lt 5 ]; then
	echo "Usage: $0 [num clients] [hostname] [client id] [request rate] [max-async] [exit after]"
	exit 1
fi

num_clients="$1"
hostname="$2"
client_id="$3"
request_rate="$4"
max_async="$5"
exit_after="$6"

for (( i = 0 ; i < $num_clients ; i++ )); do
	id=$(( client_id * num_clients + (i - num_clients) ))
	./hotstuff-client \
	--cid "$id" \
	--iter -1 \
	--max-async "$max_async" \
	--request-rate "$request_rate" \
	--exit-after "$exit_after" \
	2> "/tmp/results/$hostname-c$id.out" &
done

for (( i = 0 ; i < $num_clients ; i++ )); do
	wait
done
