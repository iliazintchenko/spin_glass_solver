import os

Ns = 10000
beta0 = 0.1
beta1 = 3.0
num_rep = 100

for ii in range(1000):

    infile = "testdata/Instances128Spins/lattice/128random" + str(ii) + ".lat"
    outfile = "testdata/Instances128Spins/results/128random" + str(ii) + ".dat"

    cmd = "./bin/main " + infile + " " + outfile + " " + str(Ns) + " " + str(beta0) + " " + str(beta1) + " " + str(num_rep)

    print 'ii:',ii

    os.system(cmd)

