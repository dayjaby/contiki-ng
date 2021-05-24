import matplotlib.pyplot as plt
import numpy as np
from collections import defaultdict
import re

received_pattern = re.compile(r"(\d\d):(\d\d).(\d\d\d).*?ID:(\d+).*?Received seq (.*?) ")
sending_pattern = re.compile(r"(\d\d):(\d\d).(\d\d\d).*?ID:(\d+).*?Sending seq (.*?) ")

def get_statistics(n=25):
    central_node = None
    received_buckets = defaultdict(set)
    received_times = defaultdict(lambda : defaultdict(list))
    send_times = dict()
    seqs = set()
    
    with open("part2_n{}.log".format(n), "r") as f:
        for line in f:
            m = received_pattern.match(line.strip())
            if m is not None:
                # e.g. 04, 08, 462, 15, 150
                minutes, seconds, milliseconds, receiver_id, seq_id = m.groups()
                receiver_id = int(receiver_id)
                seq_id = int(seq_id)
                t = int(minutes) * 60 + int(seconds) + int(milliseconds) * 0.001
                received_buckets[receiver_id].add(seq_id)
                received_times[receiver_id][seq_id].append(t)
            m = sending_pattern.match(line.strip())
            if m is not None:
                minutes, seconds, milliseconds, sender_id, seq_id = m.groups()
                sender_id = int(sender_id)
                central_node = sender_id
                seq_id = int(seq_id)
                t = int(minutes) * 60 + int(seconds) + int(milliseconds) * 0.001
                seqs.add(seq_id)
                send_times[seq_id] = t
    
    receivers = list(sorted(set(received_buckets.keys())))
    nodes = list(set(receivers)|{central_node})
    node_weights = np.ones(n)
    node_weights[central_node-1] = 0
    node_weights_latency = np.ones((n, len(seqs)))
    node_weights_latency[central_node-1,:] = 0
    received_latency = np.ndarray(shape=(len(nodes), len(seqs)), dtype=float)
    received_latency[central_node-1,:] = 0
    for r in receivers:
        for s in seqs:
            if len(received_times[r][s]) > 0:
                received_latency[r-1][s-1] = min(received_times[r][s]) - send_times[s]
            else:
                received_latency[r-1][s-1] = 0
    
    
    received_latency_avg = np.average(received_latency, axis=1)
    received_latency_std = np.std(received_latency, axis=1)
    
    received_reliability = np.ndarray(shape=(len(nodes),), dtype=float)
    for node in nodes:
        received_reliability[node-1] = len(received_buckets[node]) * 100.0 / len(seqs)
    
    # Plotting
    
    fig, (ax, ax2) = plt.subplots(2, 1, sharex=True)
    ax.errorbar(nodes, received_latency_avg, received_latency_std, linestyle='None', marker='o')
    ax2.bar(nodes, received_reliability)
    
    ax.set(xlabel='node instance', ylabel='latency in s')
    ax2.set(xlabel='node instance', ylabel='reliability in %')
    
    ax.grid()
    fig.savefig("part2_n{}.png".format(n))
    plt.show()

    avg_latency = np.average(received_latency, weights=node_weights_latency)
    var_latency = np.average((received_latency - avg_latency) ** 2, weights=node_weights_latency)
    std_latency = np.sqrt(var_latency) # have to use this to have a weighted version of np.std

    avg_reliability = np.average(received_reliability, weights=node_weights)
    var_reliability = np.average((received_reliability - avg_reliability) ** 2, weights=node_weights)
    std_reliability = np.sqrt(var_reliability)

    return avg_latency, std_latency, avg_reliability, std_reliability

experiment_sizes = [9, 25, 49]
latency_avg = np.ndarray(shape=(len(experiment_sizes),), dtype=float)
latency_std = np.ndarray(shape=(len(experiment_sizes),), dtype=float)
reliability_avg = np.ndarray(shape=(len(experiment_sizes),), dtype=float)
reliability_std = np.ndarray(shape=(len(experiment_sizes),), dtype=float)
for i, n in enumerate(experiment_sizes):
    latency_avg[i], latency_std[i], reliability_avg[i], reliability_std[i] = get_statistics(n)

fig, (ax, ax2) = plt.subplots(2, 1, sharex=True)
ax.errorbar(experiment_sizes, latency_avg, latency_std, linestyle='None', marker='o')
ax2.errorbar(experiment_sizes, reliability_avg, reliability_std, linestyle='None', marker='o')

ax.set(xlabel='number of nodes', ylabel='latency in s')
ax2.set(xlabel='number of nodes', ylabel='reliability in %')

ax.grid()
fig.savefig("part2_overview.png")
plt.show()
