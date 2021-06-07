speed = 15 # m/s
tot_dist = 6000 # m

time_increment = 0.1 # sec
nb_steps = int(tot_dist/speed/time_increment)+1

nodes = [1]
t = 0.0
x = [300]*len(nodes)
y = [100]*len(nodes)
print("# NodeID\t Time\t X\t Y\t")
for i in range(nb_steps):
    for j in range(len(nodes)):
        print(f"{int(nodes[j]-1)} {t:.1f} {x[j]:.1f} {y[j]:.1f}")
        x[j] += speed*time_increment
    t += time_increment